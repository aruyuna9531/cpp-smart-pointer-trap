#include<memory>
#include<iostream>
#include<vector>

class Base {
public:
    int i = 5;
    Base() {
        std::cout << "base constructed" << std::endl;
    }
    /* virtual */ ~Base() /* noexcept */ {     // ← 基类的析构函数定义为virtual用于一个基类（裸）指针指向派生类（堆）对象时直接对着这个基类指针delete，可以执行派生类的析构函数。（否则不会执行析构函数造成派生类内存泄漏）
        std::cout << "base destructed" << std::endl;
        // throw std::exception();      // 这里即使外面有try catch也抓不到，会直接std::terminate
    }
};

class Derived : public Base {
public:
    Derived() {
        std::cout << "derive constructed" << std::endl;
    }
    ~Derived() {
        std::cout << "derive destructed" << std::endl;
    }
};

// Base GlobalVal;  // 这个在第2条测试全局变量时解封印就行，因为这个变量会有一次构造析构函数的调用，会干扰其他的观测

void Cutline(std::string title) {
    static int i = 1;
    std::cout << "--------" << i++ << ":" << title << "--------" << std::endl;
}

// 1. delete指向派生类的基类指针
void DeleteABasePointerPointedToDerivedWithoutVirtual() {
    Cutline("delete指向派生类的基类指针");
    Base* b = new Derived();
    delete b;
    // 输出：
    // base constructed
    // derive constructed
    // base destructed
    
    // ~Derived没有被执行，Derived结构内存泄漏。（把~Base()定义为virtual可解决此问题）
}

// 2. 智能指针拥有不是分配在堆内存的变量时的反应
void WhenSharedPointerHoldPointerNotInHeap() {
    Cutline("智能指针拥有不是分配在堆内存的变量时的反应");
    // 以下是shared_ptr的报错情况。
    // unique_ptr在下述情况除了（1）和shared_ptr一样是编译错误外，其他一律是Segmentation fault，没有其他花里胡哨的报错，因此下面只列出shared_ptr的报错。

    // （1）常量：报编译错误（没有这个构造函数：no matching function for call to ‘std::shared_ptr<Base>::shared_ptr(const Base*)’
    // const Base constVal;
    // std::shared_ptr<Base> b(&constVal);

    // （2）全局变量：段错误或非法指针
    // std::shared_ptr<Base> c(&GlobalVal);
    // free(): invalid pointer / munmap_chunk(): invalid pointer / Segmentation fault (core dumped) 随机出现，原理未知
    
    // （3）静态变量：段错误
    static Base staticA;
    std::unique_ptr<Base> s(&staticA);
    // Segmentation fault (core dumped)
    
    // （4） 普通局部变量：段错误或非法指针
    Base normalA;
    std::unique_ptr<Base> n(&normalA);
    // free(): invalid pointer / munmap_chunk(): invalid pointer / Segmentation fault (core dumped) 随机出现
}

// 3. 基类智能指针拥有子类对象时的几个点（这里一律是~Base不为virtual的情况）
void BaseSmartPointerToDerived() {
    Cutline("基类智能指针拥有子类对象时的几个点");
    // (1)通过make得到指向子类对象的基类shared_ptr智能指针，生命周期结束时都会自动调用全部析构函数，不需要把基类析构函数设为虚函数。（shared_ptr持有对象的operate delete）
    std::shared_ptr<Base> ptr0 = std::make_shared<Derived>();
    // 输出：
    // base constructed
    // derive constructed
    // derive destructed
    // base destructed

    // （2.1）对shared_ptr的拷贝构造发生隐式转换时，如果入参类型是子类指针，该智能指针释放时会正常调用全部析构函数。
    std::shared_ptr<Base> ptr1(new Derived());
    // 输出：
    // base constructed
    // derive constructed
    // derive destructed
    // base destructed

    // （2.2）但显式转换入参时会产生与裸指针相同的操作。
    std::shared_ptr<Base> ptr2((Base*)(new Derived()));
    // 输出：
    // base constructed
    // derive constructed
    // base destructed

    // （3）如果智能指针拷贝构造的入参是已定义的指向子类对象的基类指针（包括new子类之后直接强转为基类指针），该智能指针释放时效果同裸指针操作。
    // 该条shared_ptr、unique_ptr通用
    Base* b = new Derived();
    std::unique_ptr<Base> ptr3(b);
    // 输出：
    // base constructed
    // derive constructed
    // base destructed

    // （4）如果是make_unique子类指针给基类unique_ptr初始化，结束时同裸指针操作。C++标准里将其判定为未定义行为。
    std::unique_ptr<Base> ptr4 = std::make_unique<Derived>();
    // 输出：
    // base constructed
    // derive constructed
    // base destructed
}

// 4. 裸指针不能拷贝到一个weak_ptr
void RawPointerCannotCopyToWeak() {
    // Base* b = new Base();
    // std::weak_ptr<Base> ptr(b); // 编译错误，没有这个构造函数 no matching function for call to ‘std::weak_ptr<Base>::weak_ptr(Base*&)’
}

// 5. 已经定义好的unique_ptr，shared_ptr，weak_ptr，只有shared_ptr可以赋值给weak_ptr，表示弱引用。其他任何不同类型的ptr都不能互相直接赋值（报编译错误）
void NoCopyConstructBetweenDifferentSmartPointer() {
    Cutline("不同类型智能指针之间只有1种情况可以赋值");

    std::shared_ptr<Base> sp = std::make_shared<Base>();
    std::weak_ptr<Base> wp;
    std::unique_ptr<Base> up = std::make_unique<Base>();

    // sp = wp;        // shared = weak 编译错误
    // sp = up;         // shared = up 编译错误（这是一个右值引用的问题 cannot bind rvalue reference of type ‘std::unique_ptr<Base>&&’ to lvalue of type ‘std::unique_ptr<Base>’。使用sp = std::move(up)可解决，此时up会因为移交了所有权被提前析构
    // wp = sp;            // weak = shared 通过，表示weak_ptr引用了shared_ptr
    // wp = up;        // weak = unique 编译错误
    // up = sp;            // unique = shared 编译错误
    // up = wp;            // unique = weak 编译错误
    std::cout << "---" << std::endl;
}

// 6. 关于不同智能指针之间move
void MoveBetweenDifferentSmartPointer() {
   

    std::shared_ptr<Base> sp = std::make_shared<Base>();
    std::weak_ptr<Base> wp;
    std::unique_ptr<Base> up = std::make_unique<Base>();

    // sp = std::move(wp);        // shared<-weak 编译错误
    // sp = std::move(up);         // shared<-unique OK
    // wp = std::move(sp);            // weak<-shared OK（shared并未立即移交所有权，效果相当于wp = sp）
    // wp = std::move(up);        // weak<- unique 编译错误
    // up = std::move(sp);            // unique <- shared 编译错误
    // up = std::move(wp);            // unique = weak 编译错误
    std::cout << "---" << std::endl;
}

// 7. make智能指针给非本类智能指针初始化的可行性
void MakeSmartPointerForEachType() {
    Cutline("make智能指针给各类智能指针初始化的可行性");

    // std::unique_ptr<Base> uu = std::make_shared<Base>();    // 编译错误，make_shared不能给unique初始化
    // std::shared_ptr<Base> us = std::make_unique<Base>();    // 通过，make_unique可以给shared初始化（唯一有意义操作）。shared_ptr的赋值运算符重载了使用unique_ptr&&作为形参的版本。
    // std::weak_ptr<Base> wu = std::make_unique<Base>();      // 编译错误，make_unique不能给weak初始化
    // std::weak_ptr<Base> ws = std::make_shared<Base>();      // 通过，但make出来的指针立即释放了，ws实质上未持有任何指针（作用可能就是跑一次构造析构函数）
    std::cout << "---" << std::endl;
}

// 8.一个已定义的裸指针对象只能被一个智能指针拷贝构造（无论是unique还是shared），拷贝的智能指针会获得这个裸指针所有权，在智能指针结束生命周期时会被立即析构，所以被多个拷贝时会发生已析构的指针被再次析构导致异常。
// shared_ptr的所谓引用计数并不适用于这个裸指针，所以用多个shared_ptr拷贝一个裸指针也是同样的情形。
void RawPointerCanOnlyBeHoldBy1SmartPointer() {
    Cutline("裸指针拷贝给智能指针的注意点");

    Base* a = new Base();
    std::shared_ptr<Base> ptr1(a);
    // std::shared_ptr<Base> ptr2(a);  // Aborted - free(): double free detected in tcache 2
}

// 9.不要显式delete一个已经被智能指针拷贝的裸指针，也不要从智能指针里取得拥有的裸指针显式delete它，会导致段错误。
void CannotDeleteARawPointerGetBySmartPointer() {
    Cutline("不要显式delete一个已经被智能指针拷贝的裸指针");

    Base* a = new Base();
    std::shared_ptr<Base> ptr1(a);
    // delete a;  // Aborted - free(): double free detected in tcache 2

    std::shared_ptr<Base> ptr2 = std::make_shared<Base>();
    // delete ptr2.get();  // Aborted - double free or corruption (out)
}

// 10.智能指针在即使发生异常时也会正常析构内存。
// 有个前提，这个异常是逻辑层面的异常比如throw exception，在外面catch异常处理的时候，会调用析构函数（析构会在catch逻辑处理之前进行）
// 如果出现系统层面的错误（段错误，oom），智能指针生命周期内显式调用std::terminate或exit(x)，或者throw之后没有地方catch直接Aborted等会直接杀进程的场合不会调用析构函数。
// 注意：如果智能指针的那个类的析构函数抛出了异常，不会被try catch接住从而直接退出。因此不要在析构函数里抛出异常。
void SmartPointerInException() {
    Cutline("智能指针在即使发生异常时也会正常析构内存。");

    try {
        std::unique_ptr<Base> a = std::make_unique<Base>();
        throw std::exception();
    } catch(std::exception&) {
        std::cout << "exception" << std::endl;
    }
    // 输出：
    // base constructed
    // base destructed
    // exception

    std::unique_ptr<Base> a2 = std::make_unique<Base>();
    // std::terminate();     // a2不会调用析构函数
    exit(0);    // a2不会调用析构函数
    // 输出：
    // base constructed
}

// 11.weak_ptr可以给shared_ptr拷贝构造初始化，效果等于给其指向的shared_ptr加引用，但必须要保证作为初始化的时候weak_ptr指向了一个活着的shared_ptr，否则会抛出异常bad_weak_ptr
void WeakPtrCopyToShared() {
    Cutline("weak_ptr可以给shared_ptr拷贝构造初始化");

    std::shared_ptr<Base> s1 = std::make_shared<Base>();
    std::weak_ptr<Base> w = s1; // w引用了s1
    std::shared_ptr<Base> s2(w);    // 等价于s2=s1
    std::cout << s2.use_count() << std::endl;   // 2
    std::cout << s2->i << std::endl;    // 5

    std::weak_ptr<Base> w2 = std::make_shared<Base>(); // w2是无效引用（参考第7点）
    std::shared_ptr<Base> s3(w2);
    std::cout << s3.use_count() << std::endl;   // terminate called after throwing an instance of 'std::bad_weak_ptr'
}

// 12. 如果一个类会作为容器模板类型，并且这个容器是存在迭代器失效情形的，注意这个类里不能定义unique_ptr成员变量。
struct VectorPtr {
    std::shared_ptr<int> p = std::make_shared<int>();   // OK，make_unique也行，只要p是shared_ptr就行
    // std::unique_ptr<int> p2 = std::make_unique<int>();   // 编译错误（push_back引起的） use of deleted function ‘VectorPtr::VectorPtr(const VectorPtr&)’ + use of deleted function ‘std::unique_ptr<_Tp, _Dp>::unique_ptr(const std::unique_ptr<_Tp, _Dp>&) [with _Tp = int; _Dp = std::default_delete<int>]’ unique_ptr删除了拷贝构造函数导致内含此类对象的类也自动删除了拷贝构造函数。
    VectorPtr() {
        std::cout << "VectorPtr constructed, address = " << p.get() << ", " << &p << ",counter = " << p.use_count() << std::endl;
    }
    ~VectorPtr() {
        std::cout << "VectorPtr destructed, address = " << p.get()  << ", " << &p << ", counter = " << p.use_count() << std::endl;
    }
};

void SharedPtrClassInVector() {
    std::vector<VectorPtr> v;
    v.push_back(VectorPtr());
    std::cout << "----" << std::endl;
    v.push_back(VectorPtr());    // cap：1→2
    std::cout << "----" << std::endl;
    v.push_back(VectorPtr());    // cap: 2→4
    std::cout << "----" << std::endl;
    v.push_back(VectorPtr());
    std::cout << "----" << std::endl;
    v.push_back(VectorPtr());    // cap: 4→8
    std::cout << "----" << std::endl;

    // 输出：
    // VectorPtr constructed, address = 0x55f7576feec0, counter = 1 // 第1个临时变量的构造
    // VectorPtr destructed, address = 0x55f7576feec0, counter = 2  // 第1个临时变量的析构，析构前引用计数为2（析构后为1，即vector里的一份引用）（*注1）
    // ----
    // VectorPtr constructed, address = 0x55f7576ff310, counter = 1 // 第2个临时变量的构造
    // VectorPtr destructed, address = 0x55f7576feec0, counter = 2  // 第1个临时变量因触发扩容析构一次。析构前计数是2（扩容移动前后的内存各有一份引用）
    // VectorPtr destructed, address = 0x55f7576ff310, counter = 2  // 第2个临时变量的析构
    // ----
    // VectorPtr constructed, address = 0x55f7576ff2f0, counter = 1 // 第3个临时变量的构造
    // VectorPtr destructed, address = 0x55f7576feec0, counter = 2 // 第1个临时变量因触发扩容析构一次
    // VectorPtr destructed, address = 0x55f7576ff310, counter = 2 // 第2个临时变量因触发扩容析构一次
    // VectorPtr destructed, address = 0x55f7576ff2f0, counter = 2 // 第3个临时变量的析构
    // ----
    // VectorPtr constructed, address = 0x55f7576ff3b0, counter = 1
    // VectorPtr destructed, address = 0x55f7576ff3b0, counter = 2  // 第4个临时变量的构造和析构，这次push没有触发扩容
    // ----
    // VectorPtr constructed, address = 0x55f7576ff3d0, counter = 1 // 第5个临时变量的构造
    // VectorPtr destructed, address = 0x55f7576feec0, counter = 2
    // VectorPtr destructed, address = 0x55f7576ff310, counter = 2
    // VectorPtr destructed, address = 0x55f7576ff2f0, counter = 2
    // VectorPtr destructed, address = 0x55f7576ff3b0, counter = 2  // 前4个临时变量触发扩容的析构（按vector内顺序）
    // VectorPtr destructed, address = 0x55f7576ff3d0, counter = 2  // 第5个临时变量的析构
    // ----
    // VectorPtr destructed, address = 0x55f7576feec0, counter = 1
    // VectorPtr destructed, address = 0x55f7576ff310, counter = 1
    // VectorPtr destructed, address = 0x55f7576ff2f0, counter = 1
    // VectorPtr destructed, address = 0x55f7576ff3b0, counter = 1
    // VectorPtr destructed, address = 0x55f7576ff3d0, counter = 1  // vector里5个变量的析构（按vector内顺序）

    // *注1：这里析构函数的调用是因为push_back这个临时变量的时候源码对这个临时变量走了一个std::move流程。address是智能指针拥有的对象的地址，不是智能指针自身的地址。智能指针自身的地址都是不一样的，扩容移动变量析构的是原来内存里的智能指针。
    // 如以下代码，会打印1次construct和2次destruct，多出来的destruct是b的析构，剩下各1次是被move的那个临时变量。use_count的打印是1
    // VectorPtr b = std::move(VectorPtr());
    // std::cout << b.p.use_count() << std::endl;
}

int main() {
    SharedPtrClassInVector();
    return 0;
}