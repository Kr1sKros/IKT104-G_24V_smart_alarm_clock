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

// Base class for all pages. All code that is overridden in the display() function will run in a seperate thread.
class Page{
public:
    void virtual display() = 0;
protected:
    Page() {}
};

// this class handles how the pages are viewed
class PageController{
private:
    std::vector<Page*> pages;
    Thread* page_thread = nullptr;
    int current_index = -1;
public:
    // used to add pages to the controller
    void add_page(Page* new_page){
        this->pages.push_back(new_page);
    }
    // returns the index of the currently displaying page
    int get_current_page(){
        return this->current_index;
    }
    // this function is used internally and externally to display a page on the lcd screen
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
    // this function is used to display the next page in the sequence
    void display_next(){
        if (this->current_index < 0 || this->current_index +1 >= this->pages.size()){
            this->display(0);
        } else {
            this->display(this->current_index +1);
        }
    }

    // this function is used to display a page and make sure that it will execute to the end before skipping
    void display_and_wait(int index){
        if (index < 0 &&  index > this->pages.size() -1){ std::cerr << ("index out of range"); }

        this->display(index);
    }
};

#endif // GRAPHICS_H