#include "pink/undo/UndoManager.h"

namespace pink
{
void UndoManager::beginNewTransaction (std::string name)
{
    pendingName = std::move (name);
    startNew = true;
}

bool UndoManager::perform (std::unique_ptr<UndoableAction> action)
{
    if (action == nullptr || ! action->perform())
        return false;

    for (auto& t : redoStack)
        used -= t.bytes;
    redoStack.clear();

    if (startNew || undoStack.empty())
    {
        undoStack.push_back ({});
        undoStack.back().name = pendingName.empty() ? std::string ("Edit") : pendingName;
        startNew = false;
    }

    auto& t = undoStack.back();
    const size_t bytes = action->getSizeInBytes();
    t.bytes += bytes;
    used += bytes;
    t.actions.push_back (std::move (action));
    enforceBudget();
    return true;
}

bool UndoManager::perform (std::function<void()> doFn, std::function<void()> undoFn, std::string transactionName)
{
    if (! transactionName.empty())
        beginNewTransaction (std::move (transactionName));
    return perform (std::make_unique<LambdaAction> (std::move (doFn), std::move (undoFn)));
}

bool UndoManager::undo()
{
    if (undoStack.empty())
        return false;

    auto t = std::move (undoStack.back());
    undoStack.pop_back();
    for (auto it = t.actions.rbegin(); it != t.actions.rend(); ++it)
        (*it)->undo();
    redoStack.push_back (std::move (t));
    startNew = true;
    return true;
}

bool UndoManager::redo()
{
    if (redoStack.empty())
        return false;

    auto t = std::move (redoStack.back());
    redoStack.pop_back();
    for (auto& a : t.actions)
        a->perform();
    undoStack.push_back (std::move (t));
    startNew = true;
    return true;
}

void UndoManager::clear()
{
    undoStack.clear();
    redoStack.clear();
    used = 0;
    startNew = true;
}

void UndoManager::enforceBudget()
{
    // drop the oldest transactions, but always keep the most recent one
    while (used > budget && undoStack.size() > 1)
    {
        used -= undoStack.front().bytes;
        undoStack.pop_front();
    }
}

} // namespace pink
