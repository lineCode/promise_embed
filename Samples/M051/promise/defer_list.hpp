#pragma once
#ifndef INC_DEFER_LIST_H_
#define INC_DEFER_LIST_H_

namespace promise{

struct defer_list{
    static void attach(pm_list *list, const Defer &defer){
        Defer *defer_ = pm_new<Defer>(defer);
        pm_list *node = &pm_memory_pool_buf_header::from_ptr(defer_)->list_;
        list->attach(node);
        pm_allocator::add_ref(defer_);
    }

    static void attach(pm_list *list, pm_list *other){
        pm_list *next = other->next();
        if(next != other){
            other->detach();
            list->attach(next);
        }
    }

    static void run(pm_list *list){
        pm_list *node = list->next();
        while(node != list){
            Defer *defer = reinterpret_cast<Defer *>(pm_memory_pool_buf_header::to_ptr(node));
            pm_list *node_next = node->next();
            node->detach();
            Defer defer_ = *defer;
            pm_allocator::dec_ref(defer);

            defer_.resolve();
            node = node_next;
        }
    }

    static inline void attach(const Defer &defer){
        attach(get_list(), defer);
    }
    static inline void attach(pm_list *other){
        attach(get_list(), other);
    }
    static void run(){
        run(get_list());
    }

private:
    static pm_list *get_list(){
        static pm_list *list = nullptr;
        if(list == nullptr)
            list = pm_stack_new<pm_list>();
        return list;
    }
};

}

#endif
