#include <iostream>
#include <fstream>
#include <vector>
#include <cstdint>
#include <chrono>
#include <string>

// Константы арифметического кодирования
const uint32_t MAX_RANGE = 65535;   // 0xFFFF
const uint32_t HALF = 32768;   // 0x8000
const uint32_t QUARTER = 16384;   // 0x4000
const uint32_t THREE_Q = 49152;   // 0xC000

// Класс для записи битов
class BitWriter {
    std::ofstream& out;
    uint8_t buffer = 0;
    int bits_used = 0;

public:
    BitWriter(std::ofstream& file) : out(file) {}

    void write_bit(bool bit) {
        buffer = (buffer << 1) | (bit ? 1 : 0);
        bits_used++;
        if (bits_used == 8) {
            out.put(static_cast<char>(buffer));
            bits_used = 0;
            buffer = 0;
        }
    }

    void write_bits(bool bit, int count) {
        for (int i = 0; i < count; i++) {
            write_bit(bit);
        }
    }

    void flush() {
        if (bits_used > 0) {
            buffer <<= (8 - bits_used);
            out.put(static_cast<char>(buffer));
        }
    }
};

// Класс для чтения битов
class BitReader {
    std::ifstream& in;
    uint8_t buffer = 0;
    int bits_left = 0;

public:
    BitReader(std::ifstream& file) : in(file) {}

    int read_bit() {
        if (bits_left == 0) {
            int byte = in.get();
            if (byte == EOF) return 0;
            buffer = static_cast<uint8_t>(byte);
            bits_left = 8;
        }
        bits_left--;
        return (buffer >> bits_left) & 1;
    }
};

// СЖАТИЕ
bool compress_file(const std::string& input_name, const std::string& output_name) {
    std::ifstream in(input_name, std::ios::binary);
    if (!in) {
        std::cerr << "Ошибка: не найден файл '" << input_name << "'\n";
        return false;
    }

    // Считаем частоты всех байтов (0–255)
    std::vector<uint32_t> count(256, 0);
    uint64_t total = 0;
    char ch;
    while (in.get(ch)) {
        count[static_cast<unsigned char>(ch)]++;
        total++;
    }
    in.close();

    // Записываем длину и таблицу частот
    std::ofstream out(output_name, std::ios::binary);
    if (!out) {
        std::cerr << "Не могу создать файл '" << output_name << "'\n";
        return false;
    }

    out.write(reinterpret_cast<const char*>(&total), sizeof(total));
    out.write(reinterpret_cast<const char*>(count.data()), 256 * sizeof(uint32_t));

    if (total == 0) {
        out.close();
        std::cout << "Файл пуст — сохранено как есть.\n";
        return true;
    }

    // Считаем начало интервала для каждого символа
    std::vector<uint32_t> start(256, 0);
    uint32_t sum = 0;
    for (int i = 0; i < 256; i++) {
        start[i] = sum;
        sum += count[i];
    }
    uint32_t total_freq = sum;

    // Арифметическое кодирование
    BitWriter writer(out);
    uint32_t low = 0, high = MAX_RANGE;
    uint32_t pending = 0;

    std::ifstream in2(input_name, std::ios::binary);
    while (in2.get(ch)) {
        unsigned char sym = static_cast<unsigned char>(ch);
        uint64_t range = static_cast<uint64_t>(high) - low + 1;

        uint32_t new_low = low + static_cast<uint32_t>((range * start[sym]) / total_freq);
        uint32_t new_high = low + static_cast<uint32_t>((range * (start[sym] + count[sym])) / total_freq) - 1;

        low = new_low;
        high = new_high;

        while (true) {
            if (high < HALF) {
                writer.write_bit(0);
                writer.write_bits(1, pending);
                pending = 0;
            }
            else if (low >= HALF) {
                writer.write_bit(1);
                writer.write_bits(0, pending);
                pending = 0;
                low -= HALF;
                high -= HALF;
            }
            else if (low >= QUARTER && high < THREE_Q) {
                pending++;
                low -= QUARTER;
                high -= QUARTER;
            }
            else {
                break;
            }

            low = (low << 1) & MAX_RANGE;
            high = ((high << 1) | 1) & MAX_RANGE;
        }
    }

    pending++;
    if (low < QUARTER) {
        writer.write_bit(0);
        writer.write_bits(1, pending);
    }
    else {
        writer.write_bit(1);
        writer.write_bits(0, pending);
    }
    writer.flush();
    out.close();

    // Статистика
    std::ifstream f1(input_name, std::ios::binary);
    std::ifstream f2(output_name, std::ios::binary);
    f1.seekg(0, std::ios::end);
    f2.seekg(0, std::ios::end);
    size_t size1 = f1.tellg();
    size_t size2 = f2.tellg();
    f1.close(); f2.close();

    double saved = (size1 > 0) ? (1.0 - (double)size2 / size1) * 100.0 : 0.0;
    std::cout << "\n✅ Сжатие завершено!\n";
    std::cout << "Было: " << size1 << " байт\n";
    std::cout << "Стало: " << size2 << " байт\n";
    std::cout << "Экономия: " << saved << "%\n";

    return true;
}

// РАСПАКОВКА
bool decompress_file(const std::string& input_name, const std::string& output_name) {
    std::ifstream in(input_name, std::ios::binary);
    if (!in) {
        std::cerr << "Ошибка: не найден файл '" << input_name << "'\n";
        return false;
    }

    uint64_t total = 0;
    std::vector<uint32_t> count(256, 0);

    in.read(reinterpret_cast<char*>(&total), sizeof(total));
    in.read(reinterpret_cast<char*>(count.data()), 256 * sizeof(uint32_t));

    if (total == 0) {
        std::ofstream out(output_name, std::ios::binary);
        out.close();
        std::cout << "Пустой файл — распакован.\n";
        return true;
    }

    // Считаем начало интервалов (как при сжатии!)
    std::vector<uint32_t> start(256, 0);
    uint32_t sum = 0;
    for (int i = 0; i < 256; i++) {
        start[i] = sum;
        sum += count[i];
    }
    uint32_t total_freq = sum;

    // Читаем первые 16 битов
    BitReader reader(in);
    uint32_t value = 0;
    for (int i = 0; i < 16; i++) {
        value = (value << 1) | reader.read_bit();
    }

    uint32_t low = 0, high = MAX_RANGE;
    std::ofstream out(output_name, std::ios::binary);
    if (!out) {
        std::cerr << "Не могу создать файл '" << output_name << "'\n";
        return false;
    }

    for (uint64_t i = 0; i < total; i++) {
        uint64_t range = static_cast<uint64_t>(high) - low + 1;
        uint64_t scaled = static_cast<uint64_t>(value - low + 1) * total_freq - 1;
        uint32_t freq_val = static_cast<uint32_t>(scaled / range);
        if (freq_val >= total_freq) freq_val = total_freq - 1;

        // Находим символ по частоте
        unsigned char sym = 0;
        for (int s = 0; s < 256; s++) {
            if (start[s] <= freq_val && freq_val < start[s] + count[s]) {
                sym = static_cast<unsigned char>(s);
                break;
            }
        }

        out.put(static_cast<char>(sym));

        // Обновляем интервал
        uint32_t new_low = low + static_cast<uint32_t>((range * start[sym]) / total_freq);
        uint32_t new_high = low + static_cast<uint32_t>((range * (start[sym] + count[sym])) / total_freq) - 1;
        low = new_low;
        high = new_high;

        // Нормализация
        while (true) {
            if (high < HALF) {
                // ничего
            }
            else if (low >= HALF) {
                value -= HALF;
                low -= HALF;
                high -= HALF;
            }
            else if (low >= QUARTER && high < THREE_Q) {
                value -= QUARTER;
                low -= QUARTER;
                high -= QUARTER;
            }
            else {
                break;
            }

            low = (low << 1) & MAX_RANGE;
            high = ((high << 1) | 1) & MAX_RANGE;
            value = (value << 1) | reader.read_bit();
        }
    }

    in.close();
    out.close();

    std::cout << "\n✅ Распаковка завершена!\n";
    std::cout << "Восстановлено " << total << " байт.\n";
    return true;
}

// ОСНОВНАЯ ПРОГРАММА
int main() {
    setlocale(0, ""); // Русский язык в консоли

    std::cout << "โปรแแกรมма арифметического сжатия\n";
    std::cout << "==================================\n";
    std::cout << "1 — Сжать файл\n";
    std::cout << "2 — Распаковать файл\n";
    std::cout << "Ваш выбор (1 или 2): ";

    int choice;
    std::cin >> choice;

    if (choice != 1 && choice != 2) {
        std::cout << "Ошибка: нужно ввести 1 или 2.\n";
        return 1;
    }

    std::string input_file, output_file;
    std::cout << "Введите имя входного файла: ";
    std::cin >> input_file;

    std::cout << "Введите имя выходного файла: ";
    std::cin >> output_file;

    auto start = std::chrono::high_resolution_clock::now();

    bool success = false;
    if (choice == 1) {
        success = compress_file(input_file, output_file);
    }
    else {
        success = decompress_file(input_file, output_file);
    }

    auto end = std::chrono::high_resolution_clock::now();
    double seconds = std::chrono::duration<double>(end - start).count();
    std::cout << "Время выполнения: " << seconds << " секунд\n";

    if (success) {
        std::cout << "Готово!\n";
    }
    else {
        std::cout << "Произошла ошибка.\n";
        return 1;
    }

    return 0;
}