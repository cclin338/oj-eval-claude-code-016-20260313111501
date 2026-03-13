#include <iostream>
#include <fstream>
#include <cstring>
#include <vector>
#include <algorithm>
#include <set>

using namespace std;

const int MAX_KEY_LEN = 65;
const int BLOCK_SIZE = 4096;
const int MAX_CHILDREN = 50;
const int MIN_CHILDREN = MAX_CHILDREN / 2;

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
    string dataFile;
    string indexFile;

    struct Node {
        bool isLeaf;
        int numKeys;
        KeyValue keys[MAX_CHILDREN];
        int children[MAX_CHILDREN + 1];
        int next;  // For leaf nodes, link to next leaf

        Node() {
            isLeaf = true;
            numKeys = 0;
            next = -1;
            memset(children, -1, sizeof(children));
        }
    };

    int rootPos;
    int nodeCount;
    fstream dataStream;
    fstream indexStream;

    void initFiles() {
        ifstream test(indexFile);
        if (!test.good()) {
            // Create new files
            ofstream d(dataFile, ios::binary);
            ofstream i(indexFile, ios::binary);
            d.close();
            i.close();

            rootPos = 0;
            nodeCount = 1;
            Node root;
            root.isLeaf = true;
            writeNode(rootPos, root);
            saveMetadata();
        } else {
            test.close();
            loadMetadata();
        }
    }

    void loadMetadata() {
        indexStream.open(indexFile, ios::in | ios::out | ios::binary);
        indexStream.seekg(0);
        indexStream.read((char*)&rootPos, sizeof(rootPos));
        indexStream.read((char*)&nodeCount, sizeof(nodeCount));
        indexStream.close();
    }

    void saveMetadata() {
        indexStream.open(indexFile, ios::in | ios::out | ios::binary);
        indexStream.seekp(0);
        indexStream.write((char*)&rootPos, sizeof(rootPos));
        indexStream.write((char*)&nodeCount, sizeof(nodeCount));
        indexStream.close();
    }

    Node readNode(int pos) {
        Node node;
        indexStream.open(indexFile, ios::in | ios::out | ios::binary);
        indexStream.seekg(sizeof(int) * 2 + pos * sizeof(Node));
        indexStream.read((char*)&node, sizeof(Node));
        indexStream.close();
        return node;
    }

    void writeNode(int pos, const Node& node) {
        indexStream.open(indexFile, ios::in | ios::out | ios::binary);
        indexStream.seekp(sizeof(int) * 2 + pos * sizeof(Node));
        indexStream.write((char*)&node, sizeof(Node));
        indexStream.close();
    }

    int allocateNode() {
        return nodeCount++;
    }

    void insertNonFull(int nodePos, const KeyValue& kv) {
        Node node = readNode(nodePos);

        if (node.isLeaf) {
            // Find insert position
            int i = node.numKeys - 1;
            while (i >= 0 && kv < node.keys[i]) {
                node.keys[i + 1] = node.keys[i];
                i--;
            }
            node.keys[i + 1] = kv;
            node.numKeys++;
            writeNode(nodePos, node);
        } else {
            // Find child to insert
            int i = node.numKeys - 1;
            while (i >= 0 && kv < node.keys[i]) {
                i--;
            }
            i++;

            Node child = readNode(node.children[i]);
            if (child.numKeys >= MAX_CHILDREN) {
                splitChild(nodePos, i);
                node = readNode(nodePos);
                if (node.keys[i] < kv) {
                    i++;
                }
            }
            insertNonFull(node.children[i], kv);
        }
    }

    void splitChild(int parentPos, int childIdx) {
        Node parent = readNode(parentPos);
        Node child = readNode(parent.children[childIdx]);

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

        writeNode(parentPos, parent);
        writeNode(parent.children[childIdx], child);
        writeNode(newNodePos, newNode);
    }

    void findInNode(int nodePos, const char* key, vector<int>& results) {
        Node node = readNode(nodePos);

        if (node.isLeaf) {
            for (int i = 0; i < node.numKeys; i++) {
                if (strcmp(node.keys[i].key, key) == 0) {
                    results.push_back(node.keys[i].value);
                }
            }
        } else {
            int i = 0;
            while (i < node.numKeys && strcmp(key, node.keys[i].key) >= 0) {
                i++;
            }
            if (i < node.numKeys + 1 && node.children[i] != -1) {
                findInNode(node.children[i], key, results);
            }
        }
    }

    bool deleteFromNode(int nodePos, const KeyValue& kv) {
        Node node = readNode(nodePos);

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
                writeNode(nodePos, node);
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
        dataFile = prefix + ".dat";
        indexFile = prefix + ".idx";
        initFiles();
    }

    ~BPlusTree() {
        saveMetadata();
    }

    void insert(const char* key, int value) {
        KeyValue kv(key, value);

        Node root = readNode(rootPos);
        if (root.numKeys >= MAX_CHILDREN) {
            int newRootPos = allocateNode();
            Node newRoot;
            newRoot.isLeaf = false;
            newRoot.numKeys = 0;
            newRoot.children[0] = rootPos;
            writeNode(newRootPos, newRoot);

            splitChild(newRootPos, 0);
            rootPos = newRootPos;
            saveMetadata();
        }

        insertNonFull(rootPos, kv);
    }

    vector<int> find(const char* key) {
        vector<int> results;
        findInNode(rootPos, key, results);
        sort(results.begin(), results.end());
        return results;
    }

    void remove(const char* key, int value) {
        KeyValue kv(key, value);
        deleteFromNode(rootPos, kv);
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
