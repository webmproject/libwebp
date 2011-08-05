// Copyright 2011 Google Inc.
//
// This code is licensed under the same terms as WebM:
//  Software License Agreement:  http://www.webmproject.org/license/software/
//  Additional IP Rights Grant:  http://www.webmproject.org/license/additional/
// -----------------------------------------------------------------------------
//
// Multi-threaded worker
//
// Author: skal@google.com (Pascal Massimino)

#include "./vp8i.h"
#include "./thread.h"

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

#ifdef WEBP_USE_THREAD

static void *WebPWorkerThreadLoop(void *ptr) {    // thread loop
  WebPWorker* const worker = (WebPWorker*)ptr;
  int done = 0;
  while (!done) {
    pthread_mutex_lock(&worker->mutex_);
    while (worker->status_ == OK) {   // wait in idling mode
      pthread_cond_wait(&worker->condition_, &worker->mutex_);
    }
    if (worker->status_ == WORK) {
      if (worker->hook) {
        worker->had_error |= !worker->hook(worker->data1, worker->data2);
      }
      worker->status_ = OK;
    } else if (worker->status_ == NOT_OK) {   // finish the worker
      done = 1;
    }
    // signal to the main thread that we're done (for Sync())
    pthread_cond_signal(&worker->condition_);
    pthread_mutex_unlock(&worker->mutex_);
  }
  return NULL;    // Thread is finished
}

// main thread state control
static void WebPWorkerChangeState(WebPWorker* const worker,
                                  WebPWorkerStatus new_status) {
  // no-op when attempting to change state on a thread that didn't come up
  if (worker->status_ < OK) return;

  pthread_mutex_lock(&worker->mutex_);
  // wait for the worker to finish
  while (worker->status_ != OK) {
    pthread_cond_wait(&worker->condition_, &worker->mutex_);
  }
  // assign new status and release the working thread if needed
  if (new_status != OK) {
    worker->status_ = new_status;
    pthread_cond_signal(&worker->condition_);
  }
  pthread_mutex_unlock(&worker->mutex_);
}

#endif

//-----------------------------------------------------------------------------

void WebPWorkerInit(WebPWorker* const worker) {
  memset(worker, 0, sizeof(*worker));
  worker->status_ = NOT_OK;
}

int WebPWorkerSync(WebPWorker* const worker) {
#ifdef WEBP_USE_THREAD
  WebPWorkerChangeState(worker, OK);
#endif
  assert(worker->status_ <= OK);
  return !worker->had_error;
}

int WebPWorkerReset(WebPWorker* const worker) {
  int ok = 1;
  worker->had_error = 0;
  if (worker->status_ < OK) {
#ifdef WEBP_USE_THREAD
    if (pthread_mutex_init(&worker->mutex_, 0) ||
        pthread_cond_init(&worker->condition_, 0)) {
      return 0;
    }
    pthread_mutex_lock(&worker->mutex_);
    ok = !pthread_create(&worker->thread_, 0, WebPWorkerThreadLoop, worker);
    if (ok) worker->status_ = OK;
    pthread_mutex_unlock(&worker->mutex_);
#else
    worker->status_ = OK;
#endif
  } else if (worker->status_ > OK) {
    ok = WebPWorkerSync(worker);
  }
  assert(!ok || (worker->status_ == OK));
  return ok;
}

void WebPWorkerLaunch(WebPWorker* const worker) {
#ifdef WEBP_USE_THREAD
  WebPWorkerChangeState(worker, WORK);
#else
  if (worker->hook)
    worker->had_error |= !worker->hook(worker->data1, worker->data2);
#endif
}

void WebPWorkerEnd(WebPWorker* const worker) {
  if (worker->status_ >= OK) {
#ifdef WEBP_USE_THREAD
    WebPWorkerChangeState(worker, NOT_OK);
    pthread_join(worker->thread_, NULL);
    pthread_mutex_destroy(&worker->mutex_);
    pthread_cond_destroy(&worker->condition_);
#else
    worker->status_ = NOT_OK;
#endif
  }
  assert(worker->status_ == NOT_OK);
}

//------------------------------------------------------------------------------

#if defined(__cplusplus) || defined(c_plusplus)
}    // extern "C"
#endif
