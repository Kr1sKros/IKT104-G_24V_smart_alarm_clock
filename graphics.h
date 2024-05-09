/*!
 * @file graphics.h
 * @brief A utility header file for all graphics related functionality.
 * @author Anders L.
 */

#ifndef GRAPHICS_H
#define GRAPHICS_H

#include <vector>
#include <iostream>

#include "mbed.h"

class Page{
public:
    void virtual display() = 0;
protected:
    Page() {}
};

class PageController{
private:
    std::vector<Page*> pages;
    Thread* page_thread = nullptr;
    int current_index = -1;
public:
    void add_page(Page* new_page){
        this->pages.push_back(new_page);
    }

    int get_current_page(){
        return this->current_index;
    }

    void display(int index){
        if (index >= 0 &&  index < this->pages.size()){
            if (this->page_thread != nullptr){
                this->page_thread->terminate();
                delete this->page_thread;
                this->page_thread = nullptr;
            }
            this->current_index = index;

            this->page_thread = new Thread(osPriorityNormal);
            this->page_thread->start(callback(this->pages[this->current_index], &Page::display));
        } else {
            std::cerr << "index out of range\n";
        }
    }

    void display_next(){
        if (this->current_index < 0 || this->current_index +1 >= this->pages.size()){
            this->display(0);
        } else {
            this->display(this->current_index +1);
        }
    }

    void display_and_wait(int index){
        if (index < 0 &&  index > this->pages.size() -1){ std::cerr << ("index out of range"); }

        this->display(index);
    }
};

#endif // GRAPHICS_H