#include <iostream>
#include <vector>
#include <map>
#include <queue>
#include <fstream>
#include <memory> 

using namespace std;

struct TreeNode {
    int freq;
    char ch;
    TreeNode* left;
    TreeNode* right;

    // Конструктор для листа
    TreeNode(char c, int f) : ch(c), freq(f), left(nullptr), right(nullptr) {}

    // Конструктор для внутреннего узла
    TreeNode(TreeNode* l, TreeNode* r) : ch(0), freq(l->freq + r->freq), left(l), right(r) {}

    ~TreeNode() {
        delete left;
        delete right;
    }
};

struct Compare {
    bool operator()(TreeNode* a, TreeNode* b) {
        return a->freq > b->freq;
    }
};

void generateCodes(TreeNode* node, const vector<bool>& code, map<char, vector<bool>>& codeMap) {
    if (!node) return;

    if (!node->left && !node->right) {
        // Лист
        codeMap[node->ch] = code;
        return;
    }

    // Влево — 0
    vector<bool> leftCode = code;
    leftCode.push_back(false);
    generateCodes(node->left, leftCode, codeMap);

    // Вправо — 1
    vector<bool> rightCode = code;
    rightCode.push_back(true);
    generateCodes(node->right, rightCode, codeMap);
}

// Запись таблицы частот + длины исходного файла
void writeHeader(ofstream& out, const map<char, int>& freqMap, size_t originalSize) {
    int size = static_cast<int>(freqMap.size());
    out.write(reinterpret_cast<const char*>(&size), sizeof(size));
    out.write(reinterpret_cast<const char*>(&originalSize), sizeof(originalSize));

    for (const auto& p : freqMap) {
        out.put(p.first);
        int freq = p.second;
        out.write(reinterpret_cast<const char*>(&freq), sizeof(freq));
    }
}

// Чтение заголовка
map<char, int> readHeader(ifstream& in, size_t& originalSize) {
    int size;
    in.read(reinterpret_cast<char*>(&size), sizeof(size));
    in.read(reinterpret_cast<char*>(&originalSize), sizeof(originalSize));

    map<char, int> freqMap;
    for (int i = 0; i < size; ++i) {
        char ch = in.get();
        int freq;
        in.read(reinterpret_cast<char*>(&freq), sizeof(freq));
        freqMap[ch] = freq;
    }
    return freqMap;
}

// Построение дерева 
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

// Кодирование файла
void encodeFile() {
    ifstream input("text.txt", ios::binary);
    if (!input) {
        cerr << "Ошибка: не найден файл text.txt\n";
        return;
    }

    // Считаем частоты и размер
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

    // Строим дерево
    unique_ptr<TreeNode> root(buildTree(freqMap));

    // Генерируем коды
    map<char, vector<bool>> codeMap;
    vector<bool> emptyCode;
    generateCodes(root.get(), emptyCode, codeMap);

    // Открываем выходной файл
    ofstream output("encoded.txt", ios::binary);
    if (!output) {
        cerr << "Ошибка: не удалось создать encoded.txt\n";
        return;
    }

    // Записываем заголовок
    writeHeader(output, freqMap, originalSize);

    // Кодируем данные
    ifstream input2("text.txt", ios::binary);
    char buffer = 0;
    int bitCount = 0;

    while (input2.get(c)) {
        const vector<bool>& bits = codeMap[c];
        for (bool bit : bits) {
            if (bit) {
                buffer |= (1 << (7 - bitCount));
            }
            bitCount++;

            if (bitCount == 8) {
                output.put(buffer);
                buffer = 0;
                bitCount = 0;
            }
        }
    }

    // Записываем остаток
    if (bitCount > 0) {
        output.put(buffer);
    }

    input2.close();
    output.close();
    cout << "Кодирование завершено. Исходный размер: " << originalSize << " байт.\n";
}

// Декодирование файла
void decodeFile() {
    ifstream input("encoded.txt", ios::binary);
    if (!input) {
        cerr << "Ошибка: не найден файл encoded.txt\n";
        return;
    }

    // Читаем заголовок
    size_t originalSize;
    map<char, int> freqMap = readHeader(input, originalSize);

    if (originalSize == 0) {
        ofstream("decoded.txt", ios::binary).close();
        cout << "Декодирование завершено: исходный файл был пуст.\n";
        return;
    }

    // Строим дерево
    unique_ptr<TreeNode> root(buildTree(freqMap));
    if (!root) {
        cerr << "Ошибка: не удалось восстановить дерево.\n";
        return;
    }

    // Декодируем
    ofstream output("decoded.txt", ios::binary);
    TreeNode* current = root.get();
    size_t decodedBytes = 0;

    char byte;
    while (input.get(byte) && decodedBytes < originalSize) {
        for (int i = 7; i >= 0 && decodedBytes < originalSize; --i) {
            bool bit = (byte >> i) & 1;

            if (bit) {
                current = current->right;
            }
            else {
                current = current->left;
            }

            if (!current->left && !current->right) {
                output.put(current->ch);
                decodedBytes++;
                current = root.get();
            }
        }
    }

    output.close();
    input.close();
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