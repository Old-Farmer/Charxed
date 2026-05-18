#pragma once

#include <functional>

#include "utils.h"

namespace charxed {

class Buffer;

enum class EditorEvent {
    kBufferRemoved,
    kEditCharEdit,
    kCommandCharEdit,
    kSearchCharEdit,

    __kCount,
};

struct EditorEventHandlerID {
    size_t id;
    size_t generation;
};

using EditorEventHandler = std::function<void(void*)>;

// A event manager which can register event callbacks.
// Users can emit event to call those callbacks;
class EditorEventManager {
   public:
    CHX_DEFAULT_CONSTRUCT_DESTRUCT(EditorEventManager);

    CHX_DELETE_COPY(EditorEventManager);
    CHX_DELETE_MOVE(EditorEventManager);

    EditorEventHandlerID AddHandler(EditorEvent event,
                                    EditorEventHandler handler);

    void RemoveHandler(EditorEvent event, EditorEventHandlerID id);

    void EmitEvent(EditorEvent event, void* arg);

   private:
    struct HandlerStore {
        EditorEventHandler handler;
        size_t generation = 0;
    };

    std::vector<HandlerStore>
        handlers_[static_cast<size_t>(EditorEvent::__kCount)];
};

}  // namespace charxed
