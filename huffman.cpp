#include <iostream>
#include <vector>
#include <map>
#include <queue>
#include <fstream>
#include <cstdint>

using namespace std;

struct TreeNode {
    int freq;
    char ch;
    TreeNode* left;
    TreeNode* right;

    TreeNode(char c, int f) : ch(c), freq(f), left(nullptr), right(nullptr) {}
    TreeNode(TreeNode* l, TreeNode* r) : ch(0), freq(l->freq + r->freq), left(l), right(r) {}

    ~TreeNode() {
        delete left;
        delete right;
    }
};

struct Compare {
    bool operator()(TreeNode* a, TreeNode* b) {
        return a->freq > b->freq; // min-heap
    }
};

void generateCodes(TreeNode* node, const vector<bool>& code, map<char, vector<bool>>& codeMap) {
    if (!node) return;

    if (!node->left && !node->right) {
        // Случай одного символа: код не должен быть пустым
        if (code.empty()) {
            codeMap[node->ch] = { false };
        }
        else {
            codeMap[node->ch] = code;
        }
        return;
    }

    vector<bool> leftCode = code;
    leftCode.push_back(false);
    generateCodes(node->left, leftCode, codeMap);

    vector<bool> rightCode = code;
    rightCode.push_back(true);
    generateCodes(node->right, rightCode, codeMap);
}

void writeHeader(ofstream& out, const map<char, int>& freqMap, size_t originalSize, int paddingBits) {
    uint32_t size = static_cast<uint32_t>(freqMap.size());
    uint64_t origSize = static_cast<uint64_t>(originalSize);
    uint32_t pad = static_cast<uint32_t>(paddingBits);

    out.write(reinterpret_cast<const char*>(&size), sizeof(size));
    out.write(reinterpret_cast<const char*>(&origSize), sizeof(origSize));
    out.write(reinterpret_cast<const char*>(&pad), sizeof(pad));

    for (const auto& p : freqMap) {
        out.put(p.first);
        uint32_t freq = static_cast<uint32_t>(p.second);
        out.write(reinterpret_cast<const char*>(&freq), sizeof(freq));
    }
}

map<char, int> readHeader(ifstream& in, size_t& originalSize, int& paddingBits) {
    uint32_t size;
    uint64_t origSize;
    uint32_t pad;

    in.read(reinterpret_cast<char*>(&size), sizeof(size));
    in.read(reinterpret_cast<char*>(&origSize), sizeof(origSize));
    in.read(reinterpret_cast<char*>(&pad), sizeof(pad));

    originalSize = static_cast<size_t>(origSize);
    paddingBits = static_cast<int>(pad);

    map<char, int> freqMap;
    for (uint32_t i = 0; i < size; ++i) {
        char ch = in.get();
        uint32_t freq;
        in.read(reinterpret_cast<char*>(&freq), sizeof(freq));
        freqMap[ch] = static_cast<int>(freq);
    }
    return freqMap;
}

TreeNode* buildTree(const map<char, int>& freqMap) {
    priority_queue<TreeNode*, vector<TreeNode*>, Compare> pq;

    for (const auto& p : freqMap) {
        pq.push(new TreeNode(p.first, p.second));
    }

    while (pq.size() > 1) {
        TreeNode* left = pq.top(); pq.pop();
        TreeNode* right = pq.top(); pq.pop();
        pq.push(new TreeNode(left, right));
    }

    return pq.empty() ? nullptr : pq.top();
}

void encodeFile() {
    // Шаг 1: Чтение файла и подсчёт частот
    ifstream input("text.txt", ios::binary);
    if (!input) {
        cerr << "Ошибка: не найден файл text.txt\n";
        return;
    }

    map<char, int> freqMap;
    size_t originalSize = 0;
    char c;
    while (input.get(c)) {
        freqMap[c]++;
        originalSize++;
    }
    input.close();

    if (originalSize == 0) {
        cout << "Файл text.txt пуст.\n";
        ofstream("encoded.txt", ios::binary).close();
        return;
    }

    // Шаг 2: Построение дерева и генерация кодов
    TreeNode* root = buildTree(freqMap);
    if (!root) {
        cerr << "Ошибка: не удалось построить дерево.\n";
        return;
    }

    map<char, vector<bool>> codeMap;
    vector<bool> emptyCode;
    generateCodes(root, emptyCode, codeMap);

    // Шаг 3: Кодирование данных в память
    ifstream input2("text.txt", ios::binary);
    vector<unsigned char> encodedBytes;
    unsigned char buffer = 0;
    int bitCount = 0;

    while (input2.get(c)) {
        const vector<bool>& bits = codeMap[c];
        for (bool bit : bits) {
            if (bit) {
                buffer |= (1 << (7 - bitCount));
            }
            bitCount++;
            if (bitCount == 8) {
                encodedBytes.push_back(buffer);
                buffer = 0;
                bitCount = 0;
            }
        }
    }

    int paddingBits = 0;
    if (bitCount > 0) {
        paddingBits = 8 - bitCount;
        encodedBytes.push_back(buffer);
    }

    input2.close();

    // Шаг 4: Запись заголовка и тела в выходной файл
    ofstream output("encoded.txt", ios::binary);
    if (!output) {
        cerr << "Ошибка: не удалось создать encoded.txt\n";
        delete root;
        return;
    }

    writeHeader(output, freqMap, originalSize, paddingBits);
    output.write(reinterpret_cast<const char*>(encodedBytes.data()), encodedBytes.size());
    output.close();

    delete root;
    cout << "Кодирование завершено. Исходный размер: " << originalSize << " байт.\n";
}

void decodeFile() {
    ifstream input("encoded.txt", ios::binary);
    if (!input) {
        cerr << "Ошибка: не найден файл encoded.txt\n";
        return;
    }

    size_t originalSize;
    int paddingBits;
    map<char, int> freqMap = readHeader(input, originalSize, paddingBits);

    if (originalSize == 0) {
        ofstream("decoded.txt", ios::binary).close();
        cout << "Декодирование завершено: исходный файл был пуст.\n";
        return;
    }

    TreeNode* root = buildTree(freqMap);
    if (!root) {
        cerr << "Ошибка: не удалось восстановить дерево.\n";
        return;
    }

    // Особый случай: файл состоит из одного символа
    if (!root->left && !root->right) {
        ofstream output("decoded.txt", ios::binary);
        for (size_t i = 0; i < originalSize; ++i) {
            output.put(root->ch);
        }
        output.close();
        delete root;
        cout << "Декодирование завершено. Восстановлено " << originalSize << " байт.\n";
        return;
    }

    // Обычное декодирование
    ofstream output("decoded.txt", ios::binary);
    TreeNode* current = root;
    size_t decodedBytes = 0;

    // Определяем, сколько битов нужно прочитать (без padding)
    input.seekg(0, ios::end);
    size_t fileSize = input.tellg();
    input.seekg(0);

    // Пропускаем заголовок
    uint32_t size;
    uint64_t origSize;
    uint32_t pad;
    input.read(reinterpret_cast<char*>(&size), sizeof(size));
    input.read(reinterpret_cast<char*>(&origSize), sizeof(origSize));
    input.read(reinterpret_cast<char*>(&pad), sizeof(pad));
    for (uint32_t i = 0; i < size; ++i) {
        input.get(); // символ
        uint32_t f;
        input.read(reinterpret_cast<char*>(&f), sizeof(f)); // частота
    }

    size_t bodySize = fileSize - static_cast<size_t>(input.tellg());
    int totalBitsToRead = static_cast<int>(bodySize * 8) - paddingBits;
    int bitsProcessed = 0;

    char byte;
    while (input.get(byte) && decodedBytes < originalSize && bitsProcessed < totalBitsToRead) {
        for (int i = 7; i >= 0; --i) {
            if (bitsProcessed >= totalBitsToRead) break;

            bool bit = (byte >> i) & 1;
            bitsProcessed++;

            if (bit) {
                current = current->right;
            }
            else {
                current = current->left;
            }

            if (!current->left && !current->right) {
                output.put(current->ch);
                decodedBytes++;
                current = root;
                if (decodedBytes >= originalSize) break;
            }
        }
    }

    output.close();
    input.close();
    delete root;
    cout << "Декодирование завершено. Восстановлено " << decodedBytes << " байт.\n";
}

int main() {
    setlocale(LC_ALL, "Russian");
    int choice;
    cout << "Выберите действие:\n";
    cout << "1 - Закодировать файл (text.txt)\n";
    cout << "2 - Раскодировать файл (encoded.txt)\n";
    cout << "> ";
    cin >> choice;

    if (choice == 1) {
        encodeFile();
    }
    else if (choice == 2) {
        decodeFile();
    }
    else {
        cout << "Неверный выбор.\n";
    }

    return 0;
}
