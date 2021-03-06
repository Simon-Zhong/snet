#include "KQueue.h"
#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>
#include <unistd.h>

namespace snet
{

KQueue::KQueue()
    : stop_(false),
      kqueue_fd_(kqueue()),
      events_(new struct kevent[kMaxEvents])
{
}

KQueue::~KQueue()
{
    if (kqueue_fd_ >= 0)
        close(kqueue_fd_);
}

void KQueue::AddEventHandler(EventHandler *eh)
{
    // Add events just the same as update events
    UpdateEvents(eh);
}

void KQueue::DelEventHandler(EventHandler *eh)
{
    struct kevent kev[2];
    int kevc = 0;

    auto fd = eh->Fd();
    auto events = static_cast<int>(eh->Events());

    if (events & static_cast<int>(Event::Read))
    {
        EV_SET(&kev[kevc], fd, EVFILT_READ, EV_DELETE, 0, 0, eh);
        ++kevc;
    }

    if (events & static_cast<int>(Event::Write))
    {
        EV_SET(&kev[kevc], fd, EVFILT_WRITE, EV_DELETE, 0, 0, eh);
        ++kevc;
    }

    if (kevc != 0)
        kevent(kqueue_fd_, kev, kevc, nullptr, 0, nullptr);

    for (int i = 0; i < kMaxEvents; ++i)
    {
        if (events_[i].udata == eh)
            events_[i].udata = nullptr;
    }
}

void KQueue::UpdateEvents(EventHandler *eh)
{
    struct kevent kev[2];
    int kevc = 0;

    auto fd = eh->Fd();
    auto events = static_cast<int>(eh->Events());
    auto enabled_events = static_cast<int>(eh->EnabledEvents());

    if (events & static_cast<int>(Event::Read))
    {
        if (enabled_events & static_cast<int>(Event::Read))
            EV_SET(&kev[kevc], fd, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, eh);
        else
            EV_SET(&kev[kevc], fd, EVFILT_READ, EV_ADD | EV_DISABLE, 0, 0, eh);
        ++kevc;
    }

    if (events & static_cast<int>(Event::Write))
    {
        if (enabled_events & static_cast<int>(Event::Write))
            EV_SET(&kev[kevc], fd, EVFILT_WRITE, EV_ADD | EV_ENABLE, 0, 0, eh);
        else
            EV_SET(&kev[kevc], fd, EVFILT_WRITE, EV_ADD | EV_DISABLE, 0, 0, eh);
        ++kevc;
    }

    if (kevc != 0)
        kevent(kqueue_fd_, kev, kevc, nullptr, 0, nullptr);
}

void KQueue::AddLoopHandler(LoopHandler *lh)
{
    lh_set_.AddLoopHandler(lh);
}

void KQueue::DelLoopHandler(LoopHandler *lh)
{
    lh_set_.DelLoopHandler(lh);
}

void KQueue::Loop()
{
    while (!stop_)
    {
        struct timespec ts;
        ts.tv_sec = 0;
        ts.tv_nsec = 20 * 1000 * 1000;

        auto kevc = kevent(kqueue_fd_, nullptr, 0,
                           events_.get(), kMaxEvents, &ts);

        for (int i = 0; i < kevc; ++i)
        {
            auto eh = static_cast<EventHandler *>(events_[i].udata);

            if (eh)
            {
                switch (events_[i].filter)
                {
                case EVFILT_READ:
                    eh->HandleRead();
                    break;

                case EVFILT_WRITE:
                    eh->HandleWrite();
                    break;
                }
            }
        }

        lh_set_.HandleLoop();
    }

    lh_set_.HandleStop();
}

void KQueue::Stop()
{
    stop_ = true;
}

} // namespace snet
