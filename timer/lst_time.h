#ifndef LST_TIMER
#define LST_TIMER

#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>

class util_timer;
struct client_data
{
    sockaddr_in address;
    int sockfd;
    util_timer *timer;
};

class util_timer
{
public:
    util_timer() : prev(NULL), next(NULL) {}

public:
    time_t expire;
    void (*cb_func)(client_data *);
    client_data *user_data;
    util_timer *prev;
    util_timer *next;
};

class sort_timer_lst
{
private:
    util_timer *head;
    util_timer *tail;

public:
    sort_timer_lst() :head(NULL), tail(NULL) {}
    ~sort_timer_lst()
    {
        util_timer *tmp = head;
        while (tmp)
        {
            head = tmp->next;
            delete tmp;
            tmp = head;
        }
    }

    void add(util_timer *time)
    {
        if (!time) return;

        if (!head) 
        {
            head = tail = time;
            return;
        } 

        if (time->expire < head->expire)
        {
            time->next = head;
            head->prev = time;
            head = time;
            return;
        }

        add(time, head);
    }

    void del(util_timer *time)
    {
        if (!time) return;

        if ((time == head) && (time == tail))
        {
            delete time;
            head = NULL;
            tail = NULL;
            return;
        }

        if (time == head)
        {
            head = head->next;
            head->prev = NULL;
            delete time;
            return;
        }

        if (time = tail)
        {
            tail = tail->prev;
            tail->next = NULL;
            delete time;
            return;
        }
        time->next->prev = time->prev;
        time->prev->next = time->next;
        delete time;
    }

    void modify(util_timer *time)
    {
        if (!time) return;

        util_timer *tmp = time->next;
        if (!tmp || (time->expire < tmp->expire)) return;

        if (head == time)
        {
            head = head->next;
            head->prev = NULL;
            time->next = NULL;
            add(time, head);
        }
        else
        {
            time->next->prev = time->prev;
            time->prev->next = time->next;
            add(time, time->next);
        }
    }

    void tick()
    {
        if (!head) return;

        time_t cur = time(NULL);
        util_timer *tmp = head;
        while(tmp)
        {
            if (cur < tmp->expire) break;

            tmp->cb_func(tmp->user_data);
            head = tmp->next;
            if (head) head->prev = NULL;

            delete tmp;
            tmp = head;

        }
    }

private:
    void add(util_timer *time, util_timer *head)
    {
        util_timer *pre = head;
        util_timer *tmp = pre->next;
        while(tmp)
        {
            if (time->expire < tmp->expire)
            {
                pre->next = time;
                time->prev = pre;
                time->next = tmp;
                tmp->prev = time;
            }
            pre = tmp;
            tmp = tmp->next;
        }
        if (!tmp)
        {
            pre->next = time;
            time->prev = pre;
            time->next = NULL;
            tail = time;
        }
    }

};

#endif