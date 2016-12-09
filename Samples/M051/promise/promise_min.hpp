#pragma once
#ifndef INC_PROMISE_MIN_HPP_
#define INC_PROMISE_MIN_HPP_

/*
 * Promise API implemented by cpp as Javascript promise style 
 *
 * Copyright (c) 2016, xhawk18
 * at gmail.com
 *
 * The MIT License (MIT)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

//#define PM_DEBUG
#define PM_EMBED_STACK 2048

#include <memory>
#include <typeinfo>
#include <algorithm>
#include <stdint.h>

#ifdef PM_DEBUG
#define PM_TYPE_NONE    0
#define PM_TYPE_TIMER   1
#define pm_assert(x)    do{ if((x) == 0) while(1); } while(0)
#else
#define pm_assert(x)    do{ } while(0)
#endif

extern "C"{
extern uint32_t g_alloc_size;
extern uint32_t g_stack_size;
}

namespace promise {

template<class P, class M>
inline size_t pm_offsetof(const M P::*member) {
    return (size_t)&reinterpret_cast<char const&>((reinterpret_cast<P*>(0)->*member));
}

template<class P, class M, class T>
inline P* pm_container_of(T* ptr, const M P::*member) {
    return reinterpret_cast<P*>(reinterpret_cast<char*>(ptr) - pm_offsetof(member));
}

template<typename T>
inline void pm_throw(const T &t){
    while(1);
}

inline constexpr size_t pm_log(size_t n) {
    return (n <= 1 ? 0 : 1 + pm_log(n >> 1));
}

template<bool match_uint8, bool match_uint16, bool match_uint32>
struct pm_offset_impl {
    typedef uint64_t type;
};
template<bool match_uint16, bool match_uint32>
struct pm_offset_impl<true, match_uint16, match_uint32> {
    typedef uint8_t type;
};
template<bool match_uint32>
struct pm_offset_impl<false, true, match_uint32> {
    typedef uint16_t type;
};
template<>
struct pm_offset_impl<false, false, true> {
    typedef uint32_t type;
};



template<size_t SIZE, size_t ADDR_ALIGN>
struct pm_offset {
    typedef typename pm_offset_impl<
        ((SIZE + ADDR_ALIGN - 1) / ADDR_ALIGN <= 0x100),
        ((SIZE + ADDR_ALIGN - 1) / ADDR_ALIGN <= 0x10000UL),
        ((SIZE + ADDR_ALIGN - 1) / ADDR_ALIGN <= 0x100000000ULL) >::type type;
};

//allocator
struct pm_stack {
#ifdef PM_EMBED_STACK
    static inline char *start() {
        static void *buf_[(PM_EMBED_STACK + sizeof(void *) - 1) / sizeof(void *)];
        return (char *)buf_;
    }

    static inline void *allocate(size_t size) {
        static char *top = start();
        char *start_ = start();

        size = (size + sizeof(void *) - 1) / sizeof(void *) * sizeof(void *);
        if (start_ + PM_EMBED_STACK < top + size)
            pm_throw("no_mem");

        void *ret = top;
        top += size;

        g_stack_size = (uint32_t)(top - start_);
        //printf("mem ======= %d %d, size = %d, %d, %d, %x\n", (int)(top - start_), (int)sizeof(void *), (int)size, OFFSET_IGNORE_BIT, (int)sizeof(itr_t), ret);
        return ret;
    }

    static const size_t OFFSET_IGNORE_BIT = pm_log(sizeof(void *));
    typedef pm_offset<PM_EMBED_STACK, sizeof(void *)>::type itr_t;
    //static const size_t OFFSET_IGNORE_BIT = 0;
    //typedef pm_offset<PM_EMBED_STACK, 1>::type itr_t;

    static inline itr_t ptr_to_itr(void *ptr) {
        if(ptr == nullptr) return (itr_t)-1;
        else return (itr_t)(((char *)ptr - (char *)pm_stack::start()) >> OFFSET_IGNORE_BIT);
    }

    static inline void *itr_to_ptr(itr_t itr) {
        if(itr == (itr_t)-1) return nullptr;
        else return (void *)((char *)pm_stack::start() + ((ptrdiff_t)itr << OFFSET_IGNORE_BIT));
    }
#else
    static const size_t OFFSET_IGNORE_BIT = 0;
    typedef void *itr_t;
    static inline itr_t ptr_to_itr(void *ptr) {
        return ptr;
    }

    static inline void *itr_to_ptr(itr_t itr) {
        return itr;
    }
#endif
};

template< class T, class... Args >
inline T *pm_stack_new(Args&&... args) {
    return new
#ifdef PM_EMBED_STACK
        (pm_stack::allocate(sizeof(T)))
#endif
        T(args...);
}


//List
struct pm_list {
    typedef pm_stack::itr_t itr_t;

    pm_list()
        : prev_(pm_stack::ptr_to_itr(reinterpret_cast<void *>(this)))
        , next_(pm_stack::ptr_to_itr(reinterpret_cast<void *>(this))) {
    }

    inline pm_list *prev() {
        return reinterpret_cast<pm_list *>(pm_stack::itr_to_ptr(prev_));
    }

    inline pm_list *next() {
        return reinterpret_cast<pm_list *>(pm_stack::itr_to_ptr(next_));
    }

    inline void prev(pm_list *other) {
        prev_ = pm_stack::ptr_to_itr(reinterpret_cast<void *>(other));
    }

    inline void next(pm_list *other) {
        next_ = pm_stack::ptr_to_itr(reinterpret_cast<void *>(other));
    }

    /* Connect or disconnect two lists. */
    static void toggleConnect(pm_list *list1, pm_list *list2) {
        pm_list *prev1 = list1->prev();
        pm_list *prev2 = list2->prev();
        prev1->next(list2);
        prev2->next(list1);
        list1->prev(prev2);
        list2->prev(prev1);
    }

    /* Connect two lists. */
    static void connect(pm_list *list1, pm_list *list2) {
        toggleConnect(list1, list2);
    }

    /* Disconnect tow lists. */
    static void disconnect(pm_list *list1, pm_list *list2) {
        toggleConnect(list1, list2);
    }

    /* Same as listConnect */
    void attach(pm_list *node) {
        connect(this, node);
    }

    /* Make node in detach mode */
    void detach () {
        disconnect(this, this->next());
    }

    /* Move node to list, after moving,
       node->next == this
       this->prev == node
     */
    void move(pm_list *node) {
#if 1
        node->prev()->next(node->next());
        node->next()->prev(node->prev());

        node->next(this);
        node->prev(this->prev());
        this->prev()->next(node);
        this->prev(node);
#else
        node->detach();
        attach(node);
#endif
    }

    /* Check if list is empty */
    int empty() {
        return (this->next() == this);
    }

private:
    itr_t prev_;
    itr_t next_;
};


struct pm_memory_pool {
    pm_list free_;
    size_t size_;
    pm_memory_pool(size_t size)
        : free_()
        , size_(size){
    }
};

//allocator
struct pm_memory_pool_buf_header {
    pm_memory_pool_buf_header(pm_memory_pool *pool)
        : pool_(pm_stack::ptr_to_itr(reinterpret_cast<void *>(pool)))
        , ref_count_(0){
    }

    pm_list list_;
    pm_stack::itr_t pool_;
    int16_t ref_count_;

    static inline void *to_ptr(pm_memory_pool_buf_header *header) {
        struct dummy_pool_buf {
            pm_memory_pool_buf_header header_;
            struct {
                void *buf_[1];
            } buf_;
        };
        dummy_pool_buf *buf = reinterpret_cast<dummy_pool_buf *>(
            reinterpret_cast<char *>(header) - pm_offsetof(&dummy_pool_buf::header_));
        return (void *)&buf->buf_;
    }

    static inline void *to_ptr(pm_list *list){
        pm_memory_pool_buf_header *header = pm_container_of(list, &pm_memory_pool_buf_header::list_);
        return pm_memory_pool_buf_header::to_ptr(header);
    }

    static inline pm_memory_pool_buf_header *from_ptr(void *ptr) {
        struct dummy_pool_buf {
            pm_memory_pool_buf_header header_;
            struct {
                void *buf_[1];
            } buf_;
        };
        dummy_pool_buf *buf = pm_container_of(ptr, &dummy_pool_buf::buf_);
        return &buf->header_;
    }
};

template <size_t SIZE>
struct pm_memory_pool_buf {
    struct buf_t {
        buf_t() {}
        void *buf[(SIZE + sizeof(void *) - 1) / sizeof(void *)];
    };

    pm_memory_pool_buf(pm_memory_pool *pool)
        : header_(pool) {
    }

    pm_memory_pool_buf_header header_;
    buf_t buf_;
};

template <size_t SIZE>
struct pm_size_allocator {
    static inline pm_memory_pool *get_memory_pool() {
        static pm_memory_pool *pool_ = nullptr;
        if(pool_ == nullptr)
            pool_ = pm_stack_new<pm_memory_pool>(SIZE);
        return pool_;
    }
};

struct pm_allocator {
private:
    template <size_t SIZE>
    static void *obtain_impl() {
        g_alloc_size += SIZE;
        pm_memory_pool *pool = pm_size_allocator<SIZE>::get_memory_pool();
        if (pool->free_.empty()) {
            pm_memory_pool_buf<SIZE> *pool_buf = 
                pm_stack_new<pm_memory_pool_buf<SIZE>>(pool);
            //printf("++++ obtain = %p %d\n", (void *)&pool_buf->buf_, sizeof(T));
            return (void *)&pool_buf->buf_;
        }
        else {
            pm_list *node = pool->free_.next();
            node->detach();
            pm_memory_pool_buf_header *header = pm_container_of(node, &pm_memory_pool_buf_header::list_);
            pm_memory_pool_buf<SIZE> *pool_buf = pm_container_of
                (header, &pm_memory_pool_buf<SIZE>::header_);
            //printf("++++ obtain = %p %d\n", (void *)&pool_buf->buf_, sizeof(T));
            return (void *)&pool_buf->buf_;
        }
    }

    static void release(void *ptr) {
        //printf("--- release = %p\n", ptr);
        pm_memory_pool_buf_header *header = pm_memory_pool_buf_header::from_ptr(ptr);
        pm_memory_pool *pool = reinterpret_cast<pm_memory_pool *>(pm_stack::itr_to_ptr(header->pool_));
        pool->free_.move(&header->list_);
        g_alloc_size -= pool->size_;
    }

    static void add_ref_impl(void *object) {
        //printf("add_ref %p\n", object);
        if (object != nullptr) {
            pm_memory_pool_buf_header *header = pm_memory_pool_buf_header::from_ptr(object);
            //printf("++ %p %d -> %d\n", pool_buf, pool_buf->ref_count_, pool_buf->ref_count_ + 1);
            ++header->ref_count_;
        }
    }

    static bool dec_ref_impl(void *object) {
        //printf("dec_ref %p\n", object);
        if (object != nullptr) {
            pm_memory_pool_buf_header *header = pm_memory_pool_buf_header::from_ptr(object);
            //printf("-- %p %d -> %d\n", pool_buf, pool_buf->ref_count_, pool_buf->ref_count_ - 1);
            pm_assert(header->ref_count_ > 0);
            --header->ref_count_;
            if (header->ref_count_ == 0) {
                pm_allocator::release(object);
                return true;
            }
        }
        return false;
    }

public:
    template <typename T>
    static inline void *obtain() {
        return obtain_impl<sizeof(T)>();
    }

    template<typename T>
    static void add_ref(T *object) {
        add_ref_impl(reinterpret_cast<void *>(const_cast<T *>(object)));
    }

    template<typename T>
    static void dec_ref(T *object) {
        if(dec_ref_impl(reinterpret_cast<void *>(const_cast<T *>(object)))){
            object->~T();
            //pm_allocator::release(reinterpret_cast<void *>(const_cast<T *>(object)));
        }
    }
};

template< class T, class... Args >
inline T *pm_new(Args&&... args) {
    T *object = new(pm_allocator::template obtain<T>()) T(args...);
    pm_allocator::add_ref(object);
    return object;
}

template< class T >
inline void pm_delete(T *object){
    pm_allocator::dec_ref(object);
}

template< class T >
class pm_shared_ptr {
public:
    ~pm_shared_ptr() {
        pm_allocator::dec_ref(object_);
    }

    explicit pm_shared_ptr(T *object)
        : object_(object) {
    }

    explicit pm_shared_ptr()
        : object_(nullptr) {
    }

    pm_shared_ptr(pm_shared_ptr const &ptr)
        : object_(ptr.object_) {
        pm_allocator::add_ref(object_);
    }

    pm_shared_ptr &operator=(pm_shared_ptr const &ptr) {
        pm_shared_ptr(ptr).swap(*this);
        return *this;
    }

    bool operator==(pm_shared_ptr const &ptr) const {
        return object_ == ptr.object_;
    }

    bool operator!=(pm_shared_ptr const &ptr) const {
        return !(*this == ptr);
    }

    bool operator==(T const *ptr) const {
        return object_ == ptr;
    }

    bool operator!=(T const *ptr) const {
        return !(*this == ptr);
    }

    inline T *operator->() const {
        return object_;
    }

    inline T *obtain_rawptr() {
        pm_allocator::add_ref(object_);
        return object_;
    }

    inline void release_rawptr() {
        pm_allocator::dec_ref(object_);
    }

    void clear() {
        pm_shared_ptr().swap(*this);
    }

//private:

    inline void swap(pm_shared_ptr &ptr) {
        std::swap(object_, ptr.object_);
    }

    T *object_;
};

template< class T, class... Args >
inline pm_shared_ptr<T> pm_make_shared(Args&&... args) {
    return pm_shared_ptr<T>(pm_new<T>(args...));
}

template< class T, class B, class... Args >
inline pm_shared_ptr<B> pm_make_shared2(Args&&... args) {
    return pm_shared_ptr<B>(pm_new<T>(args...));
}

template<typename FUNC>
struct func_traits {
    typedef decltype((*(FUNC*)0)()) ret_type;
};


struct Promise;

template<typename T>
class pm_shared_ptr_promise {
    typedef pm_shared_ptr_promise Defer;
public:
    ~pm_shared_ptr_promise() {
        pm_allocator::dec_ref(object_);
    }

    explicit pm_shared_ptr_promise(T *object)
        : object_(object) {
    }

    explicit pm_shared_ptr_promise()
        : object_(nullptr) {
    }

    pm_shared_ptr_promise(pm_shared_ptr_promise const &ptr)
        : object_(ptr.object_) {
        pm_allocator::add_ref(object_);
    }

    pm_shared_ptr_promise(pm_shared_ptr<T> const &ptr)
        : object_(ptr.object_) {
        pm_allocator::add_ref(object_);
    }

    Defer &operator=(Defer const &ptr) {
        Defer(ptr).swap(*this);
        return *this;
    }

    bool operator==(Defer const &ptr) const {
        return object_ == ptr.object_;
    }

    bool operator!=(Defer const &ptr) const {
        return !(*this == ptr);
    }

    bool operator==(T const *ptr) const {
        return object_ == ptr;
    }

    bool operator!=(T const *ptr) const {
        return !(*this == ptr);
    }

    inline T *operator->() const {
        return object_;
    }

    inline T *obtain_rawptr() {
        pm_allocator::add_ref(object_);
        return object_;
    }

    inline void release_rawptr() {
        pm_allocator::dec_ref(object_);
    }

    Defer find_pending() const {
        return object_->find_pending();
    }
    
    void reject_pending() {
        if(object_ != nullptr)
            object_->reject_pending();
    }

    void clear() {
        Defer().swap(*this);
    }

    void resolve() const {
        object_->resolve();
    }

    void reject() const {
        object_->reject();
    }

    Defer then(Defer &promise) {
        return object_->then(promise);
    }

    template <typename FUNC_ON_RESOLVED, typename FUNC_ON_REJECTED>
    Defer then(FUNC_ON_RESOLVED on_resolved, FUNC_ON_REJECTED on_rejected) const {
        return object_->template then<FUNC_ON_RESOLVED, FUNC_ON_REJECTED>(on_resolved, on_rejected);
    }

    template <typename FUNC_ON_RESOLVED>
    Defer then(FUNC_ON_RESOLVED on_resolved) const {
        return object_->template then<FUNC_ON_RESOLVED>(on_resolved);
    }

    template <typename FUNC_ON_REJECTED>
    Defer fail(FUNC_ON_REJECTED on_rejected) const {
        return object_->template fail<FUNC_ON_REJECTED>(on_rejected);
    }

    template <typename FUNC_ON_ALWAYS>
    Defer always(FUNC_ON_ALWAYS on_always) const {
        return object_->template always<FUNC_ON_ALWAYS>(on_always);
    }

    template <typename FUNC_ON_BYPASS>
    Defer bypass(FUNC_ON_BYPASS on_bypass) const {
        return object_->template bypass<FUNC_ON_BYPASS>(on_bypass);
    }

private:
    inline void swap(Defer &ptr) {
        std::swap(object_, ptr.object_);
    }

    T *object_;
};

typedef pm_shared_ptr_promise<Promise> Defer;

typedef void(*FnSimple)();

template <typename RET, typename FUNC>
struct ResolveChecker;
template <typename RET, typename FUNC>
struct RejectChecker;

inline Defer reject(void);

template <typename Promise, typename FUNC_ON_RESOLVED, typename FUNC_ON_REJECTED>
struct PromiseEx 
    : public Promise {
    typedef typename func_traits<FUNC_ON_RESOLVED>::ret_type resolve_ret_type;
    typedef typename func_traits<FUNC_ON_REJECTED>::ret_type reject_ret_type;

    struct {
        void *buf[(sizeof(FUNC_ON_RESOLVED) + sizeof(void *) - 1)/ sizeof(void *)];
    } on_resolved_;
    struct {
        void *buf[(sizeof(FUNC_ON_REJECTED) + sizeof(void *) - 1) / sizeof(void *)];
    } on_rejected_;

    PromiseEx(const FUNC_ON_RESOLVED &on_resolved, const FUNC_ON_REJECTED &on_rejected)
        : Promise() {
        //printf("self = %d, %d %d\n", (int)sizeof(*this), (int)sizeof(on_resolved_), (int)sizeof(on_rejected_));
        reinterpret_cast<void *>(new(&on_resolved_) FUNC_ON_RESOLVED(on_resolved));
        reinterpret_cast<void *>(new(&on_rejected_) FUNC_ON_REJECTED(on_rejected));
    }

    virtual ~PromiseEx() {
        if(!Promise::func_cleared)
            clear_func_impl();
    }

    virtual void clear_func_impl() {
        reinterpret_cast<FUNC_ON_RESOLVED *>(&on_resolved_)->~FUNC_ON_RESOLVED();
        reinterpret_cast<FUNC_ON_REJECTED *>(&on_rejected_)->~FUNC_ON_REJECTED();
    }

    virtual Defer call_resolve(Defer &self) {
        const FUNC_ON_RESOLVED &on_resolved = *reinterpret_cast<FUNC_ON_RESOLVED *>(&on_resolved_);
        return ResolveChecker<resolve_ret_type, FUNC_ON_RESOLVED>::call(on_resolved, self);
    }
    virtual Defer call_reject(Defer &self) {
        const FUNC_ON_REJECTED &on_rejected = *reinterpret_cast<FUNC_ON_REJECTED *>(&on_rejected_);
        return RejectChecker<reject_ret_type, FUNC_ON_REJECTED>::call(on_rejected, self);
    }
};

struct Promise {
    Defer next_;
    pm_stack::itr_t prev_;

    enum status_t {
        kInit       = 0,
        kResolved   = 1,
        kRejected   = 2,
        kFinished   = 3
    };
    uint8_t status_      ;//: 2;
    uint8_t func_cleared ;//: 1;

#ifdef PM_DEBUG
    uint32_t type_;
#endif

    Promise(const Promise &) = delete;
    explicit Promise()
        : next_(nullptr)
        , prev_(pm_stack::ptr_to_itr(nullptr))
        , status_(kInit)
        , func_cleared(0)
#ifdef PM_DEBUG
        , type_(PM_TYPE_NONE)
#endif
        {
        //printf("size promise = %d %d %d\n", (int)sizeof(*this), (int)sizeof(prev_), (int)sizeof(next_));
        //printf("prev_ = %x %x, start = %x\n", (int)prev_, pm_stack::itr_to_ptr(prev_), pm_stack::start());
    }

    virtual ~Promise() {
        if (next_.operator->()) {
            next_->prev_ = pm_stack::ptr_to_itr(nullptr);
        }
    }

    void prepare_resolve() {
        if (status_ != kInit) return;
        status_ = kResolved;
    }

    void resolve() {
        prepare_resolve();
        if(status_ == kResolved)
            call_next();
    }

    void prepare_reject() {
        if (status_ != kInit) return;
        status_ = kRejected;
    }

    void reject() {
        prepare_reject();
        if(status_ == kRejected)
            call_next();
    }

    virtual Defer call_resolve(Defer &self) = 0;
    virtual Defer call_reject(Defer &self) = 0;
    virtual void clear_func_impl() = 0;

    void clear_func() {
        if(!func_cleared) {
            func_cleared = 1;
            clear_func_impl();
        }
    }

    template <typename FUNC>
    void run(FUNC func, Defer d) {
        func(d);
    }

    Defer call_next() {
        if(status_ == kResolved) {
            if(next_.operator->()){
                pm_allocator::add_ref(this);
                status_ = kFinished;
                Defer d = next_->call_resolve(next_);
                next_->clear_func();
                if(d.operator->())
                    d->call_next();
                //next_.clear();
                pm_allocator::dec_ref(this);
                return d;
            }
        }
        else if(status_ == kRejected ) {
            if(next_.operator->()){
                pm_allocator::add_ref(this);
                status_ = kFinished;
                Defer d =  next_->call_reject(next_);
                next_->clear_func();
                if (d.operator->())
                    d->call_next();
                //next_.clear();
                pm_allocator::dec_ref(this);
                return d;
            }
        }

        return next_;
    }

    Defer then(Defer &promise) {
        joinDeferObject(this, promise);
        //printf("2prev_ = %d %x %x\n", (int)promise->prev_, pm_stack::itr_to_ptr(promise->prev_), this);
        return call_next();
    }

    template <typename FUNC_ON_RESOLVED, typename FUNC_ON_REJECTED>
    Defer then(FUNC_ON_RESOLVED on_resolved, FUNC_ON_REJECTED on_rejected) {
        Defer promise = pm_make_shared2<PromiseEx<Promise, FUNC_ON_RESOLVED, FUNC_ON_REJECTED>, Promise>(on_resolved, on_rejected);
        return then(promise);
    }

    template <typename FUNC_ON_RESOLVED>
    Defer then(FUNC_ON_RESOLVED on_resolved) {
        return then<FUNC_ON_RESOLVED, FnSimple>(on_resolved, nullptr);
    }

    template <typename FUNC_ON_REJECTED>
    Defer fail(FUNC_ON_REJECTED on_rejected) {
        return then<FnSimple, FUNC_ON_REJECTED>(nullptr, on_rejected);
    }

    template <typename FUNC_ON_ALWAYS>
    Defer always(FUNC_ON_ALWAYS on_always) {
        return then<FUNC_ON_ALWAYS, FUNC_ON_ALWAYS>(on_always, on_always);
    }

    template <typename FUNC_ON_BYPASS>
    Defer bypass(FUNC_ON_BYPASS on_bypass) {
        struct on_resolved {
            FUNC_ON_BYPASS on_bypass_;
            on_resolved(const FUNC_ON_BYPASS &on_bypass)
                : on_bypass_(on_bypass){
            }
            void operator()() const {
                on_bypass_();
            }
        };

        struct on_rejected {
            FUNC_ON_BYPASS on_bypass_;
            on_rejected(const FUNC_ON_BYPASS &on_bypass)
                : on_bypass_(on_bypass){
            }
            Defer operator()() const {
                on_bypass_();
                return promise::reject();
            }
        };
        return then(on_resolved(on_bypass), on_rejected(on_bypass));
    }


    Defer find_pending() {
        if (status_ == kInit) {
            Promise *p = this;
            Promise *prev = static_cast<Promise *>(pm_stack::itr_to_ptr(p->prev_));
            //printf("3prev_ = %d %x\n", (int)p->prev_, pm_stack::itr_to_ptr(p->prev_));
            while (prev != nullptr) {
                if (prev->status_ != kInit)
                    return prev->next_;
                p = prev;
                //printf("4prev_ = %d %x\n", (int)p->prev_, pm_stack::itr_to_ptr(p->prev_));
                prev = static_cast<Promise *>(pm_stack::itr_to_ptr(p->prev_));
            }

            pm_allocator::add_ref(p);
            return Defer(p);
        }
        else {
            Promise *p = this;
            Promise *next = p->next_.operator->();
            while (next != nullptr) {
                if (next->status_ == kInit)
                    return p->next_;
                p = next;
                next = p->next_.operator->();
            }
            return Defer();
        }
    }

    void reject_pending(){
        Defer pending = find_pending();
        if(pending.operator->() != nullptr)
            pending.reject();
    }

    static Promise *get_head(Promise *p){
        while(p){
            Promise *prev = static_cast<Promise *>(pm_stack::itr_to_ptr(p->prev_));
            if(prev == nullptr) break;
            p = prev;
        }
        return p;
    }
    static Promise *get_tail(Promise *p){
        if(p == nullptr)
            while(1);
        while(p){
            Defer &next = p->next_;
            if(next.operator->() == nullptr) break;
            p = next.operator->();
        }
        return p;
    }
    
    
    static inline void joinDeferObject(Promise *self, Defer &next){
        Promise *head = get_head(next.operator->());
        Promise *tail = get_tail(next.operator->());

        if(self->next_.operator->()){
            self->next_->prev_ = pm_stack::ptr_to_itr(reinterpret_cast<void *>(tail));
            //printf("5prev_ = %d %x\n", (int)self->next_->prev_, pm_stack::itr_to_ptr(self->next_->prev_));
        }
        tail->next_ = self->next_;
        pm_allocator::add_ref(head);
        self->next_ = Defer(head);
        head->prev_ = pm_stack::ptr_to_itr(reinterpret_cast<void *>(self));
        //printf("6prev_ = %d %x\n", (int)next->prev_, pm_stack::itr_to_ptr(next->prev_));
    }

    static inline void joinDeferObject(Defer &self, Defer &next){
        joinDeferObject(self.operator->(), next);
    }
};


template <typename RET, typename FUNC>
struct ResolveChecker {
    static Defer call(const FUNC &func, Defer &self) {
        func();
        self->prepare_resolve();
        return self;
    }
};

template <typename FUNC>
struct ResolveChecker<Defer, FUNC> {
    static Defer call(const FUNC &func, Defer &self) {
        Defer ret = func();
        Promise::joinDeferObject(self, ret);
        return ret;
    }
};


template <typename RET>
struct ResolveChecker<RET, FnSimple> {
    static Defer call(const FnSimple &func, Defer &self) {
        if (func != nullptr)
            func();
        self->prepare_resolve();
        return self;
    }
};


template <typename RET, typename FUNC>
struct RejectChecker {
    static Defer call(const FUNC &func, Defer &self) {
        func();
        self->prepare_resolve();
        return self;
    }
};

template <typename FUNC>
struct RejectChecker<Defer, FUNC> {
    static Defer call(const FUNC &func, Defer &self) {
        Defer ret = func();
        Promise::joinDeferObject(self, ret);
        return ret;
    }
};

template <typename RET>
struct RejectChecker<RET, FnSimple> {
    static Defer call(const FnSimple &func, Defer &self) {
        if (func != nullptr) {
            func();
            self->prepare_resolve();
            return self;
        }
        self->prepare_reject();
        return self;
    }
};

/* Create new promise object */
template <typename FUNC>
inline Defer newPromise(FUNC func) {
    Defer promise = pm_make_shared2<PromiseEx<Promise, FnSimple, FnSimple>, Promise>(nullptr, nullptr);
    promise->run(func, promise);
    return promise;
}

/* Loop while func call resolved */
template <typename FUNC>
inline Defer While(FUNC func) {
    return newPromise(func).then([func]() {
        return While(func);
    });
}

/* Return a rejected promise directly */
inline Defer reject(){
    return newPromise([](Defer &d){ d.reject(); });
}

}

#include "defer_list.hpp"
#include "timer.hpp"
#include "irq.hpp"

#endif
