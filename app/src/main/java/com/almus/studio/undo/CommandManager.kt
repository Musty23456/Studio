package com.almus.studio.undo

/**
 * A reversible edit action. Each command captures enough state on creation
 * to fully reverse itself, so [undo] never needs to guess prior state.
 */
interface Command {
    val label: String
    fun apply()
    fun undo()
}

/**
 * Classic two-stack undo/redo manager. Every mutation made through the
 * editor (trim, split, move, volume change, add/remove track, etc.) should
 * be wrapped in a [Command] and pushed here rather than mutating the
 * project model directly, so every edit is undoable.
 */
class CommandManager(private val maxHistory: Int = 200) {

    private val undoStack = ArrayDeque<Command>()
    private val redoStack = ArrayDeque<Command>()

    val canUndo: Boolean get() = undoStack.isNotEmpty()
    val canRedo: Boolean get() = redoStack.isNotEmpty()
    val undoLabel: String? get() = undoStack.lastOrNull()?.label
    val redoLabel: String? get() = redoStack.lastOrNull()?.label

    fun execute(command: Command) {
        command.apply()
        undoStack.addLast(command)
        if (undoStack.size > maxHistory) undoStack.removeFirst()
        redoStack.clear()
    }

    fun undo(): Boolean {
        val command = undoStack.removeLastOrNull() ?: return false
        command.undo()
        redoStack.addLast(command)
        return true
    }

    fun redo(): Boolean {
        val command = redoStack.removeLastOrNull() ?: return false
        command.apply()
        undoStack.addLast(command)
        return true
    }

    fun clear() {
        undoStack.clear()
        redoStack.clear()
    }
}

/** Convenience command built from two lambdas, for simple field mutations. */
class LambdaCommand(
    override val label: String,
    private val doAction: () -> Unit,
    private val undoAction: () -> Unit
) : Command {
    override fun apply() = doAction()
    override fun undo() = undoAction()
}
