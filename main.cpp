#include <iostream>
#include <fstream>
#include <cstring>
#include <vector>
#include <algorithm>
#include <map>
#include <set>

using namespace std;

const int MAX_KEY_LEN = 65;
const int ORDER = 100;  // B+ tree order

struct KeyValue {
    char key[MAX_KEY_LEN];
    int value;

    KeyValue() {
        memset(key, 0, sizeof(key));
        value = 0;
    }

    KeyValue(const char* k, int v) {
        strncpy(key, k, MAX_KEY_LEN - 1);
        key[MAX_KEY_LEN - 1] = '\0';
        value = v;
    }

    bool operator<(const KeyValue& other) const {
        int cmp = strcmp(key, other.key);
        if (cmp != 0) return cmp < 0;
        return value < other.value;
    }

    bool operator==(const KeyValue& other) const {
        return strcmp(key, other.key) == 0 && value == other.value;
    }
};

class BPlusTree {
private:
    struct Node {
        bool isLeaf;
        int numKeys;
        KeyValue keys[ORDER];
        int children[ORDER + 1];
        int next;

        Node() {
            isLeaf = true;
            numKeys = 0;
            next = -1;
            for (int i = 0; i <= ORDER; i++) {
                children[i] = -1;
            }
        }
    };

    string indexFile;
    fstream indexStream;
    int rootPos;
    int nodeCount;
    map<int, Node> cache;  // Cache nodes in memory
    bool dirty;

    void openStream() {
        if (!indexStream.is_open()) {
            indexStream.open(indexFile, ios::in | ios::out | ios::binary);
        }
    }

    void closeStream() {
        if (indexStream.is_open()) {
            indexStream.close();
        }
    }

    void initFiles() {
        ifstream test(indexFile);
        if (!test.good()) {
            test.close();
            // Create new file
            ofstream create(indexFile, ios::binary);
            create.close();

            rootPos = 0;
            nodeCount = 1;
            Node root;
            root.isLeaf = true;
            cache[rootPos] = root;
            saveMetadata();
            saveAllNodes();
        } else {
            test.close();
            loadMetadata();
        }
    }

    void loadMetadata() {
        openStream();
        indexStream.seekg(0);
        indexStream.read((char*)&rootPos, sizeof(rootPos));
        indexStream.read((char*)&nodeCount, sizeof(nodeCount));
        closeStream();
    }

    void saveMetadata() {
        openStream();
        indexStream.seekp(0);
        indexStream.write((char*)&rootPos, sizeof(rootPos));
        indexStream.write((char*)&nodeCount, sizeof(nodeCount));
        closeStream();
    }

    Node& getNode(int pos) {
        if (cache.find(pos) != cache.end()) {
            return cache[pos];
        }

        Node node;
        openStream();
        indexStream.seekg(sizeof(int) * 2 + pos * sizeof(Node));
        indexStream.read((char*)&node, sizeof(Node));
        closeStream();

        cache[pos] = node;
        return cache[pos];
    }

    void saveNode(int pos) {
        if (cache.find(pos) == cache.end()) return;

        openStream();
        indexStream.seekp(sizeof(int) * 2 + pos * sizeof(Node));
        indexStream.write((char*)&cache[pos], sizeof(Node));
        closeStream();
    }

    void saveAllNodes() {
        for (auto& p : cache) {
            saveNode(p.first);
        }
        saveMetadata();
    }

    int allocateNode() {
        return nodeCount++;
    }

    void insertNonFull(int nodePos, const KeyValue& kv) {
        Node& node = getNode(nodePos);

        if (node.isLeaf) {
            int i = node.numKeys - 1;
            while (i >= 0 && kv < node.keys[i]) {
                node.keys[i + 1] = node.keys[i];
                i--;
            }
            node.keys[i + 1] = kv;
            node.numKeys++;
            dirty = true;
        } else {
            int i = node.numKeys - 1;
            while (i >= 0 && kv < node.keys[i]) {
                i--;
            }
            i++;

            Node& child = getNode(node.children[i]);
            if (child.numKeys >= ORDER) {
                splitChild(nodePos, i);
                if (!(getNode(nodePos).keys[i] < kv) && !(kv < getNode(nodePos).keys[i])) {
                    // Equal, go right
                    i++;
                } else if (kv < getNode(nodePos).keys[i]) {
                    // Less than, stay left
                } else {
                    // Greater than, go right
                    i++;
                }
            }
            insertNonFull(node.children[i], kv);
        }
    }

    void splitChild(int parentPos, int childIdx) {
        Node& parent = getNode(parentPos);
        Node& child = getNode(parent.children[childIdx]);

        int newNodePos = allocateNode();
        Node newNode;
        newNode.isLeaf = child.isLeaf;

        int mid = child.numKeys / 2;
        newNode.numKeys = child.numKeys - mid;

        for (int i = 0; i < newNode.numKeys; i++) {
            newNode.keys[i] = child.keys[mid + i];
        }

        if (!child.isLeaf) {
            for (int i = 0; i <= newNode.numKeys; i++) {
                newNode.children[i] = child.children[mid + i];
            }
        } else {
            newNode.next = child.next;
            child.next = newNodePos;
        }

        child.numKeys = mid;

        // Insert new child in parent
        for (int i = parent.numKeys; i > childIdx; i--) {
            parent.children[i + 1] = parent.children[i];
            if (i > 0) parent.keys[i] = parent.keys[i - 1];
        }

        parent.children[childIdx + 1] = newNodePos;
        parent.keys[childIdx] = newNode.keys[0];
        parent.numKeys++;

        cache[newNodePos] = newNode;
        dirty = true;
    }

    void findInNode(int nodePos, const char* key, vector<int>& results) {
        Node& node = getNode(nodePos);

        if (node.isLeaf) {
            for (int i = 0; i < node.numKeys; i++) {
                if (strcmp(node.keys[i].key, key) == 0) {
                    results.push_back(node.keys[i].value);
                }
            }
            // Check next leaf node if exists
            if (node.next != -1) {
                Node& next = getNode(node.next);
                if (next.numKeys > 0 && strcmp(next.keys[0].key, key) == 0) {
                    findInNode(node.next, key, results);
                }
            }
        } else {
            int i = 0;
            while (i < node.numKeys) {
                int cmp = strcmp(key, node.keys[i].key);
                if (cmp < 0) {
                    break;
                }
                i++;
            }
            if (node.children[i] != -1) {
                findInNode(node.children[i], key, results);
            }
        }
    }

    bool deleteFromNode(int nodePos, const KeyValue& kv) {
        Node& node = getNode(nodePos);

        if (node.isLeaf) {
            int i = 0;
            while (i < node.numKeys && !(node.keys[i] == kv)) {
                i++;
            }

            if (i < node.numKeys) {
                for (int j = i; j < node.numKeys - 1; j++) {
                    node.keys[j] = node.keys[j + 1];
                }
                node.numKeys--;
                dirty = true;
                return true;
            }
            return false;
        } else {
            int i = 0;
            while (i < node.numKeys && node.keys[i] < kv) {
                i++;
            }

            if (node.children[i] != -1) {
                return deleteFromNode(node.children[i], kv);
            }
            return false;
        }
    }

public:
    BPlusTree(const string& prefix) {
        indexFile = prefix + ".idx";
        dirty = false;
        initFiles();
    }

    ~BPlusTree() {
        if (dirty) {
            saveAllNodes();
        }
    }

    void insert(const char* key, int value) {
        KeyValue kv(key, value);

        Node& root = getNode(rootPos);
        if (root.numKeys >= ORDER) {
            int newRootPos = allocateNode();
            Node newRoot;
            newRoot.isLeaf = false;
            newRoot.numKeys = 0;
            newRoot.children[0] = rootPos;
            cache[newRootPos] = newRoot;

            splitChild(newRootPos, 0);
            rootPos = newRootPos;
        }

        insertNonFull(rootPos, kv);
        saveAllNodes();
        dirty = false;
        cache.clear();  // Clear cache to save memory
    }

    vector<int> find(const char* key) {
        vector<int> results;
        findInNode(rootPos, key, results);
        sort(results.begin(), results.end());
        results.erase(unique(results.begin(), results.end()), results.end());
        cache.clear();  // Clear cache
        return results;
    }

    void remove(const char* key, int value) {
        KeyValue kv(key, value);
        deleteFromNode(rootPos, kv);
        saveAllNodes();
        dirty = false;
        cache.clear();  // Clear cache
    }
};

int main() {
    ios_base::sync_with_stdio(false);
    cin.tie(nullptr);

    BPlusTree tree("bptree");

    int n;
    cin >> n;

    for (int i = 0; i < n; i++) {
        string cmd;
        cin >> cmd;

        if (cmd == "insert") {
            string key;
            int value;
            cin >> key >> value;
            tree.insert(key.c_str(), value);
        } else if (cmd == "delete") {
            string key;
            int value;
            cin >> key >> value;
            tree.remove(key.c_str(), value);
        } else if (cmd == "find") {
            string key;
            cin >> key;
            vector<int> results = tree.find(key.c_str());
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
