package com.almus.studio

import com.almus.studio.undo.CommandManager
import com.almus.studio.undo.LambdaCommand
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class CommandManagerTest {

    @Test
    fun `execute applies the command and enables undo`() {
        val manager = CommandManager()
        var value = 0
        manager.execute(LambdaCommand("Increment", { value++ }, { value-- }))
        assertEquals(1, value)
        assertTrue(manager.canUndo)
        assertFalse(manager.canRedo)
    }

    @Test
    fun `undo reverts the most recent command`() {
        val manager = CommandManager()
        var value = 0
        manager.execute(LambdaCommand("Increment", { value++ }, { value-- }))
        manager.undo()
        assertEquals(0, value)
        assertFalse(manager.canUndo)
        assertTrue(manager.canRedo)
    }

    @Test
    fun `redo reapplies an undone command`() {
        val manager = CommandManager()
        var value = 0
        manager.execute(LambdaCommand("Increment", { value++ }, { value-- }))
        manager.undo()
        manager.redo()
        assertEquals(1, value)
        assertTrue(manager.canUndo)
        assertFalse(manager.canRedo)
    }

    @Test
    fun `new command after undo clears redo stack`() {
        val manager = CommandManager()
        var value = 0
        manager.execute(LambdaCommand("A", { value += 1 }, { value -= 1 }))
        manager.undo()
        manager.execute(LambdaCommand("B", { value += 10 }, { value -= 10 }))
        assertFalse(manager.canRedo)
        assertEquals(10, value)
    }

    @Test
    fun `history respects max size`() {
        val manager = CommandManager(maxHistory = 3)
        var value = 0
        repeat(5) { i ->
            manager.execute(LambdaCommand("cmd$i", { value += 1 }, { value -= 1 }))
        }
        // Only the most recent 3 commands should be undoable.
        var undone = 0
        while (manager.undo()) undone++
        assertEquals(3, undone)
    }

    @Test
    fun `undo on empty stack returns false`() {
        val manager = CommandManager()
        assertFalse(manager.undo())
    }
}
