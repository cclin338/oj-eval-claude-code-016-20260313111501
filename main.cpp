#include <iostream>
#include <fstream>
#include <cstring>
#include <vector>
#include <algorithm>
#include <map>
#include <set>

using namespace std;

const int MAX_KEY_LEN = 65;

// Simpler approach: use std::map to maintain sorted order
// and flush to disk only at program end
class FileStorage {
private:
    string dataFile;
    map<pair<string, int>, bool> data;  // (key, value) -> exists
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

            data[make_pair(string(key), value)] = true;
        }

        in.close();
    }

    void saveToFile() {
        if (!modified) return;

        ofstream out(dataFile, ios::binary);

        for (auto& p : data) {
            if (p.second) {  // Only save if exists
                const string& key = p.first.first;
                int value = p.first.second;

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
        auto p = make_pair(key, value);
        if (data.find(p) == data.end() || !data[p]) {
            data[p] = true;
            modified = true;
        }
    }

    vector<int> find(const string& key) {
        vector<int> results;
        for (auto& p : data) {
            if (p.second && p.first.first == key) {
                results.push_back(p.first.second);
            }
        }
        return results;
    }

    void remove(const string& key, int value) {
        auto p = make_pair(key, value);
        if (data.find(p) != data.end() && data[p]) {
            data[p] = false;
            modified = true;
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
