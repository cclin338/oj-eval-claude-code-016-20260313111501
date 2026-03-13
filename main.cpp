#include <iostream>
#include <fstream>
#include <cstring>
#include <vector>
#include <algorithm>
#include <map>
#include <set>

using namespace std;

const int MAX_KEY_LEN = 65;

// Better approach: group values by key for faster find operations
class FileStorage {
private:
    string dataFile;
    map<string, set<int>> data;  // key -> set of values
    bool modified;

    void loadFromFile() {
        ifstream in(dataFile, ios::binary);
        if (!in.good()) {
            return;
        }

        while (in.peek() != EOF) {
            int keyLen;
            if (!in.read((char*)&keyLen, sizeof(keyLen))) break;

            char key[MAX_KEY_LEN];
            if (!in.read(key, keyLen)) break;
            key[keyLen] = '\0';

            int value;
            if (!in.read((char*)&value, sizeof(value))) break;

            data[string(key)].insert(value);
        }

        in.close();
    }

    void saveToFile() {
        if (!modified) return;

        ofstream out(dataFile, ios::binary);

        for (auto& p : data) {
            const string& key = p.first;
            for (int value : p.second) {
                int keyLen = key.length();
                out.write((char*)&keyLen, sizeof(keyLen));
                out.write(key.c_str(), keyLen);
                out.write((char*)&value, sizeof(value));
            }
        }

        out.close();
    }

public:
    FileStorage(const string& filename) : dataFile(filename), modified(false) {
        loadFromFile();
    }

    ~FileStorage() {
        saveToFile();
    }

    void insert(const string& key, int value) {
        if (data[key].insert(value).second) {
            modified = true;
        }
    }

    vector<int> find(const string& key) {
        auto it = data.find(key);
        if (it == data.end()) {
            return vector<int>();
        }
        return vector<int>(it->second.begin(), it->second.end());
    }

    void remove(const string& key, int value) {
        auto it = data.find(key);
        if (it != data.end()) {
            if (it->second.erase(value) > 0) {
                modified = true;
                if (it->second.empty()) {
                    data.erase(it);
                }
            }
        }
    }
};

int main() {
    ios_base::sync_with_stdio(false);
    cin.tie(nullptr);

    FileStorage storage("storage.dat");

    int n;
    cin >> n;

    for (int i = 0; i < n; i++) {
        string cmd;
        cin >> cmd;

        if (cmd == "insert") {
            string key;
            int value;
            cin >> key >> value;
            storage.insert(key, value);
        } else if (cmd == "delete") {
            string key;
            int value;
            cin >> key >> value;
            storage.remove(key, value);
        } else if (cmd == "find") {
            string key;
            cin >> key;
            vector<int> results = storage.find(key);
            if (results.empty()) {
                cout << "null\n";
            } else {
                for (size_t j = 0; j < results.size(); j++) {
                    if (j > 0) cout << " ";
                    cout << results[j];
                }
                cout << "\n";
            }
        }
    }

    return 0;
}
