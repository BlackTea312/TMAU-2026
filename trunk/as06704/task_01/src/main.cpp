#define NOMINMAX

#include "csv_writer.h"
#include "input_signal.h"
#include "model.h"
#include "simulator.h"

#include <windows.h>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <limits>
#include <memory>
#include <string>
#include <vector>

namespace {

template <typename T>
T readNumber(const std::string& prompt) {
    T value{};
    while (true) {
        std::cout << prompt;
        if (std::cin >> value) {
            return value;
        }
        std::cout << "Некорректный ввод. Попробуйте еще раз.\n";
        std::cin.clear();
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    }
}

int readChoice(const std::string& prompt, int minValue, int maxValue) {
    while (true) {
        const int choice = readNumber<int>(prompt);
        if (choice >= minValue && choice <= maxValue) {
            return choice;
        }
        std::cout << "Введите число от " << minValue << " до "
                  << maxValue << ".\n";
    }
}

std::unique_ptr<InputSignal> createSignal(int choice) {
    switch (choice) {
    case 1:
        return std::make_unique<StepSignal>();
    case 2:
        return std::make_unique<ImpulseSignal>();
    default:
        return std::make_unique<HarmonicSignal>();
    }
}

std::filesystem::path findPlotScript() {
    wchar_t exePath[MAX_PATH] = {};
    const DWORD length = GetModuleFileNameW(nullptr, exePath, MAX_PATH);

    if (length == 0 || length >= MAX_PATH) {
        return {};
    }

    std::filesystem::path directory =
        std::filesystem::path(exePath).parent_path();

    while (!directory.empty()) {
        const std::filesystem::path candidate = directory / L"plot.py";
        std::error_code error;

        if (std::filesystem::is_regular_file(candidate, error)) {
            return candidate;
        }

        const std::filesystem::path parent = directory.parent_path();
        if (parent == directory) {
            break;
        }
        directory = parent;
    }

    return {};
}

void launchPlot(const std::filesystem::path& plotPath) {
    std::cout << "\nЗапуск построения графика командой py plot.py...\n";

    const std::wstring command = L"py \"" + plotPath.wstring() + L"\"";
    const int result = _wsystem(command.c_str());

    if (result != 0) {
        std::cout << "Не удалось построить график.\n"
                  << "Проверьте установку Python и Matplotlib.\n"
                  << "Установка Matplotlib: py -m pip install matplotlib\n";
    }
}

}

int main() {
    SetConsoleCP(65001);
    SetConsoleOutputCP(65001);
    std::cout << "Выберите модель:\n";
    std::cout << "1. Model 1.2 — линейная модель с задержкой управления\n";
    std::cout << "2. Model 2.6 — нелинейная полиномиальная модель\n";
    std::cout << "3. Model 3.10 — дифференциальное уравнение\n";
    const int modelChoice = readChoice("Номер модели (например, 1): ", 1, 3);

    std::cout << "\nВыберите входное воздействие:\n";
    std::cout << "1. Ступенчатое: u(τ) = 1\n";
    std::cout << "2. Импульсное: u(0) = 1, далее 0\n";
    std::cout << "3. Гармоническое: u(τ) = sin(τ)\n";
    const int signalChoice = readChoice("Номер сигнала (например, 2): ", 1, 3);

    const int enteredSteps = readNumber<int>(
        "Количество шагов n (например, 50): "
    );
    if (enteredSteps < 1 || enteredSteps > 100000) {
        std::cout << "Количество шагов должно быть от 1 до 100000.\n";
        return 1;
    }

    const std::size_t steps = static_cast<std::size_t>(enteredSteps);
    const double initialY = readNumber<double>(
        "Начальное значение y(0) (например, 0): "
    );

    std::unique_ptr<Model> model;
    double dt = 1.0;

    if (modelChoice == 1) {
        const double a = readNumber<double>(
            "Коэффициент a (например, 0.8): "
        );
        const double b = readNumber<double>(
            "Коэффициент b (например, 0.2): "
        );
        int delay = readNumber<int>(
            "Задержка k, целое число >= 1 (например, 1): "
        );
        while (delay < 1) {
            std::cout << "Задержка должна быть не меньше 1.\n";
            delay = readNumber<int>("Задержка k (например, 1): ");
        }
        model = std::make_unique<LinearModel12>(
            a, b, static_cast<std::size_t>(delay)
        );
    } else if (modelChoice == 2) {
        const double a1 = readNumber<double>(
            "Коэффициент a1 (например, 0.1): "
        );
        const double a2 = readNumber<double>(
            "Коэффициент a2 (например, 0.2): "
        );
        const double b = readNumber<double>(
            "Коэффициент b (например, 0.1): "
        );
        model = std::make_unique<NonlinearModel26>(a1, a2, b);
    } else {
        const double a = readNumber<double>(
            "Коэффициент a (например, 0.5): "
        );
        const double b = readNumber<double>(
            "Коэффициент b (например, 0.2): "
        );
        do {
            dt = readNumber<double>(
                "Шаг интегрирования dt > 0 (например, 0.1): "
            );
            if (dt <= 0.0) {
                std::cout << "Шаг интегрирования должен быть положительным.\n";
            }
        } while (dt <= 0.0);
        model = std::make_unique<DifferentialModel310>(a, b);
    }

    const std::unique_ptr<InputSignal> signal = createSignal(signalChoice);

    std::cout << "\nВыбрана модель: " << model->name() << '\n';
    std::cout << "Входное воздействие: " << signal->name() << '\n';
    std::cout << "Число шагов: " << steps
              << ", y(0) = " << initialY << '\n';
    if (modelChoice == 3) {
        std::cout << "Шаг интегрирования dt = " << dt << '\n';
    }

    const std::string stability = model->stabilityInfo();
    if (!stability.empty()) {
        std::cout << '\n' << stability << '\n';
    }

    Simulator simulator;
    const std::vector<Sample> samples = simulator.run(
        *model, *signal, steps, initialY, dt
    );
    if (samples.empty()) {
        std::cout << "Нет результатов для сохранения.\n";
        return 1;
    }

    CsvWriter::printTable(samples);

    const std::filesystem::path plotPath = findPlotScript();
    std::filesystem::path csvPath;

    if (!plotPath.empty()) {
        csvPath = plotPath.parent_path() / L"results.csv";
    } else {
        csvPath = std::filesystem::current_path() / L"results.csv";
        std::cout << "\nПредупреждение: plot.py не найден.\n"
                  << "CSV будет сохранен в рабочую папку программы.\n";
    }

    if (!CsvWriter::save(csvPath.string(), samples,
                         model->name(), signal->name())) {
        std::cout << "Не удалось сохранить CSV-файл: "
                  << csvPath.string() << '\n';
        return 1;
    }

    std::cout << "\nТаблица сохранена в файл:\n"
              << csvPath.string() << '\n';

    if (!plotPath.empty()) {
        launchPlot(plotPath);
    } else {
        std::cout << "Построение графика пропущено: файл plot.py не найден.\n";
    }

    return 0;
}
