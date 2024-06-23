//
// Created by leipeng on 2022-09-22.
//

#include <boost/context/continuation.hpp>
#include <stdio.h>

struct Node {
    Node* left;
    int data;
    Node* right;
};
using boost::context::continuation;
using boost::context::callcc;
template<class SeqValue> class Generator {
    continuation src, sink;
    SeqValue result;
public:
    template<class Func> explicit Generator(Func src_fn) {
        this->src = callcc([src_fn,this](continuation&& _sink) {
            this->sink = std::move(_sink);
            src_fn(*this);
            return std::move(this->sink);
        });
    }
    void yield(const SeqValue& v) { result = v; sink = sink.resume(); }
    Generator& operator++() { src = src.resume(); return *this; }
    const SeqValue& operator*() const { return result; }
    explicit operator bool() const { return static_cast<bool>(src); }
};
void walk_g(Generator<int>& g, Node* p) {
    if (p) {
        walk_g(g, p->left);
        g.yield(p->data);
        walk_g(g, p->right);
    }
}
void use_walk_g(Node* root) {
    Generator<int> src([root](Generator<int>& g) { walk_g(g, root); });
    for (; src; ++src)
        printf("%d\n", *src);
    printf("callcc: %s passed\n", __func__);
}

//-------------------------------------------------------------------------

void walk_fn(Node* p, Node** seq_value, continuation& sink) {
    if (p) {
        walk_fn(p->left, seq_value, sink);
        *seq_value = p; // 这两句相当于
        sink = sink.resume(); // 一般 coroutine 的 yield p
        walk_fn(p->right, seq_value, sink);
    }
}

void use_walk_fn(Node* root) {
    Node* curr = nullptr;
    auto src = callcc([root,&curr](continuation&& sink) {
        walk_fn(root, &curr, sink);
        return std::move(sink);
    });
    while (src) {
        printf("%d\n", curr->data);
        src = src.resume();
    }
    printf("callcc: %s passed\n", __func__);
}

int main() {
    Node* root = new Node{
        new Node{
            new Node{
                nullptr,
                1,
                nullptr,
            },
            3,
            new Node{
                nullptr,
                4,
                nullptr,
            },
        },
        5,
        new Node{
            new Node{
                nullptr,
                6,
                nullptr,
            },
            7,
            new Node{
                nullptr,
                8,
                nullptr,
            }
        },
    };
    use_walk_fn(root);
    use_walk_g(root);
    return 0;
}
