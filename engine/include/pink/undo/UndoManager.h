// PINKSTUDIO - unlimited undo/redo (bounded only by a configurable memory budget).
// Actions are grouped into transactions so one user gesture undoes as a unit.
#pragma once

#include <cstddef>
#include <deque>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace pink
{
class UndoableAction
{
public:
    virtual ~UndoableAction() = default;
    virtual bool perform() = 0;
    virtual bool undo() = 0;
    /** Rough memory cost, used to enforce the budget. */
    virtual size_t getSizeInBytes() const { return 64; }
};

/** Convenience action built from two lambdas. */
class LambdaAction final : public UndoableAction
{
public:
    LambdaAction (std::function<void()> doFn, std::function<void()> undoFn, size_t sizeBytes = 64)
        : doIt (std::move (doFn)), undoIt (std::move (undoFn)), size (sizeBytes) {}
    bool perform() override { doIt(); return true; }
    bool undo() override { undoIt(); return true; }
    size_t getSizeInBytes() const override { return size; }

private:
    std::function<void()> doIt, undoIt;
    size_t size;
};

class UndoManager
{
public:
    explicit UndoManager (size_t memoryBudgetBytes = 256u * 1024u * 1024u) : budget (memoryBudgetBytes) {}

    /** Starts a new transaction; following perform() calls are grouped until the next call. */
    void beginNewTransaction (std::string name);

    /** Performs the action and records it. Clears the redo history. */
    bool perform (std::unique_ptr<UndoableAction> action);
    bool perform (std::function<void()> doFn, std::function<void()> undoFn, std::string transactionName = {});

    bool undo();
    bool redo();
    bool canUndo() const noexcept { return ! undoStack.empty(); }
    bool canRedo() const noexcept { return ! redoStack.empty(); }
    std::string getUndoDescription() const { return canUndo() ? undoStack.back().name : std::string(); }
    std::string getRedoDescription() const { return canRedo() ? redoStack.back().name : std::string(); }
    size_t getNumUndoTransactions() const noexcept { return undoStack.size(); }
    size_t getMemoryUsed() const noexcept { return used; }
    void clear();

private:
    struct Transaction
    {
        std::string name;
        std::vector<std::unique_ptr<UndoableAction>> actions;
        size_t bytes = 0;
    };

    void enforceBudget();

    std::deque<Transaction> undoStack, redoStack;
    std::string pendingName;
    bool startNew = true;
    size_t budget, used = 0;
};

} // namespace pink
