/* Cattle -- Flexible Brainfuck interpreter library
 * Copyright (C) 2008  Andrea Bolognani <eof@kiyuko.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Homepage: http://www.kiyuko.org/software/cattle
 */

#include "cattle-enums.h"
#include "cattle-instruction.h"

/**
 * SECTION:cattle-instruction
 * @short_description: Single instruction
 * @see_also: #CattleProgram
 *
 * #CattleInstruction represents a single Brainfuck instruction, repeated one
 * or more times in a row.
 *
 * Multiple instructions of the same type (i.e. multiple increment
 * instruction) are grouped together to reduce memory usage and execution
 * time.
 *
 * Consider the following, very common Brainfuck code:
 *
 * <informalexample><programlisting>
 * +++.-----
 * </programlisting></informalexample>
 *
 * As you can see, it is constituted by three increment instructions, a
 * print instruction, and five decrement instructions.
 *
 * Instead of creating nine separate objects, with all the involved overhead,
 * we can create just three objects (one with value
 * #CATTLE_INSTRUCTION_INCREASE, one with value #CATTLE_INSTRUCTION_PRINT, and
 * one with value #CATTLE_INSTRUCTION_DECREASE) and set their quantity to
 * three, one and five respectively.
 *
 * Each instruction maintains a reference to the next instruction in the
 * execution flow, and to the first instruction in the loop (if the value is
 * #CATTLE_INSTRUCTION_LOOP_BEGIN).
 */

G_DEFINE_TYPE (CattleInstruction, cattle_instruction, G_TYPE_OBJECT)

/**
 * CattleInstructionValue:
 * @CATTLE_INSTRUCTION_NONE: do nothing.
 * @CATTLE_INSTRUCTION_MOVE_LEFT: move the tape to the left.
 * @CATTLE_INSTRUCTION_MOVE_RIGHT: move the tape to the right.
 * @CATTLE_INSTRUCTION_INCREASE: increase the current value.
 * @CATTLE_INSTRUCTION_DECREASE: decrease the current value.
 * @CATTLE_INSTRUCTION_LOOP_BEGIN: execute the loop until the current value is
 * zero, then proceed to the next instruction.
 * @CATTLE_INSTRUCTION_LOOP_END: exit from the currently-executing loop.
 * @CATTLE_INSTRUCTION_READ: get one character from the input and save its
 * value at the current position.
 * @CATTLE_INSTRUCTION_PRINT: send the current value to the output.
 * @CATTLE_INSTRUCTION_DUMP_TAPE: dump the whole content of the tape on the
 * output.
 *
 * Brainfuck instructions supported by Cattle, as #gunichar<!-- -->s.
 *
 * #CATTLE_INSTRUCTION_DUMP_TAPE is not part of the Brainfuck language, but
 * it's often used for debugging and implemented in many interpreters, so
 * it's included in Cattle as well.
 */

/**
 * CattleInstruction:
 *
 * Opaque data structure representing an instruction. It should never be
 * accessed directly; use the methods below instead.
 */

#define CATTLE_INSTRUCTION_GET_PRIVATE(object) (G_TYPE_INSTANCE_GET_PRIVATE ((object), CATTLE_TYPE_INSTRUCTION, CattleInstructionPrivate))

struct _CattleInstructionPrivate {
    gboolean disposed;

    CattleInstructionValue    value;
    gint                      quantity;
    CattleInstruction        *next;
    CattleInstruction        *loop;
};

/* Properties */
enum {
    PROP_0,
    PROP_VALUE,
    PROP_QUANTITY,
    PROP_NEXT,
    PROP_LOOP
};

static void   cattle_instruction_set_property   (GObject        *object,
                                                 guint           property_id,
                                                 const GValue   *value,
                                                 GParamSpec     *pspec);
static void   cattle_instruction_get_property   (GObject        *object,
                                                 guint           property_id,
                                                 GValue         *value,
                                                 GParamSpec     *pspec);

static void
cattle_instruction_init (CattleInstruction *self)
{
    self->priv = CATTLE_INSTRUCTION_GET_PRIVATE (self);

    self->priv->value = CATTLE_INSTRUCTION_NONE;
    self->priv->quantity = 1;
    self->priv->next = NULL;
    self->priv->loop = NULL;

    self->priv->disposed = FALSE;
}

static void
cattle_instruction_dispose (GObject *object)
{
    CattleInstruction *self = CATTLE_INSTRUCTION (object);

    if (G_LIKELY (!self->priv->disposed)) {

        self->priv->disposed = TRUE;

        if (G_IS_OBJECT (self->priv->next)) {
            g_object_unref (G_OBJECT (self->priv->next));
            self->priv->next = NULL;
        }
        if (G_IS_OBJECT (self->priv->loop)) {

            /* Releasing the first instruction in the loop causes all the
             * instructions in the loop to be disposed */
            g_object_unref (G_OBJECT (self->priv->loop));
            self->priv->loop = NULL;
        }

        G_OBJECT_CLASS (cattle_instruction_parent_class)->dispose (G_OBJECT (self));
    }
}

static void
cattle_instruction_finalize (GObject *object)
{
    CattleInstruction *self = CATTLE_INSTRUCTION (object);

    G_OBJECT_CLASS (cattle_instruction_parent_class)->finalize (G_OBJECT (self));
}

static void
cattle_instruction_class_init (CattleInstructionClass *self)
{
    GObjectClass *object_class = G_OBJECT_CLASS (self);
    GParamSpec *pspec;

    object_class->set_property = cattle_instruction_set_property;
    object_class->get_property = cattle_instruction_get_property;
    object_class->dispose = cattle_instruction_dispose;
    object_class->finalize = cattle_instruction_finalize;

    /**
     * CattleInstruction:value:
     *
     * Value of the instruction. Accepted values are in the
     * #CattleInstructionValue enumeration.
     *
     * Changes to this property are not notified.
     */
    pspec = g_param_spec_enum ("value",
                               "Value of the instruction",
                               "Get/set instruction's value",
                               CATTLE_TYPE_INSTRUCTION_VALUE,
                               CATTLE_INSTRUCTION_NONE,
                               G_PARAM_READWRITE);
    g_object_class_install_property (object_class,
                                     PROP_VALUE,
                                     pspec);

    /**
     * CattleInstruction:quantity:
     *
     * Number of times the instruction has to be executed.
     *
     * Changes to this property are not notified.
     */
    pspec = g_param_spec_int ("quantity",
                              "Number of times the instruction has to be executed",
                              "Get/set instruction's quantity",
                              0,
                              G_MAXINT,
                              1,
                              G_PARAM_READWRITE);
    g_object_class_install_property (object_class,
                                     PROP_QUANTITY,
                                     pspec);

    /**
     * CattleInstruction:next:
     *
     * Next instruction in the execution flow. Can be %NULL if there are no
     * more instructions to be executed.
     *
     * Changes to this property are not notified.
     */
    pspec = g_param_spec_object ("next",
                                 "Next instruction to be executed",
                                 "Get/set next instruction",
                                 CATTLE_TYPE_INSTRUCTION,
                                 G_PARAM_READWRITE);
    g_object_class_install_property (object_class,
                                     PROP_NEXT,
                                     pspec);

    /**
     * CattleInstruction:loop:
     *
     * Instructions to be executed in the loop. Should be %NULL unless the
     * value of the instruction is #CATTLE_INSTRUCTION_LOOP_BEGIN.
     *
     * Changes to this property are not notified.
     */
    pspec = g_param_spec_object ("loop",
                                 "Instructions executed in the loop",
                                 "Get/set loop code",
                                 CATTLE_TYPE_INSTRUCTION,
                                 G_PARAM_READWRITE);
    g_object_class_install_property (object_class,
                                     PROP_LOOP,
                                     pspec);

    g_type_class_add_private (object_class, sizeof (CattleInstructionPrivate));
}

static void
cattle_instruction_set_property (GObject        *object,
                                 guint           property_id,
                                 const GValue   *value,
                                 GParamSpec     *pspec)
{
    CattleInstruction *self = CATTLE_INSTRUCTION (object);

    if (G_LIKELY (!self->priv->disposed)) {

        switch (property_id) {

            case PROP_VALUE:
                cattle_instruction_set_value (self, g_value_get_enum (value));
                break;

            case PROP_QUANTITY:
                cattle_instruction_set_quantity (self, g_value_get_int (value));
                break;

            case PROP_NEXT:
                cattle_instruction_set_next (self, g_value_get_object (value));
                break;

            case PROP_LOOP:
                cattle_instruction_set_loop (self, g_value_get_object (value));
                break;

            default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        }
    }
}

static void
cattle_instruction_get_property (GObject      *object,
                                 guint         property_id,
                                 GValue       *value,
                                 GParamSpec   *pspec)
{
    CattleInstruction *self = CATTLE_INSTRUCTION (object);

    if (G_LIKELY (!self->priv->disposed)) {

        switch (property_id) {

            case PROP_VALUE:
                g_value_set_enum (value, cattle_instruction_get_value (self));
                break;

            case PROP_QUANTITY:
                g_value_set_int (value, cattle_instruction_get_quantity (self));
                break;

            case PROP_NEXT:
                g_value_set_object (value, cattle_instruction_get_next (self));
                break;

            case PROP_LOOP:
                g_value_set_object (value, cattle_instruction_get_loop (self));
                break;

            default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        }
    }
}

/**
 * cattle_instruction_new:
 *
 * Create and initialize a new instruction.
 *
 * The newly-created instruction has a #CattleInstruction:quantity of one,
 * and its #CattleInstruction:value is #CATTLE_INSTRUCTION_NONE.
 *
 * Returns: a new #CattleInstruction.
 **/
CattleInstruction*
cattle_instruction_new (void)
{
    return g_object_new (CATTLE_TYPE_INSTRUCTION, NULL);
}

/**
 * cattle_instruction_set_value:
 * @instruction: a #CattleInstruction
 * @value: instruction to be executed
 *
 * Set the value of @instruction. Accepted values are from the
 * #CattleInstructionValue enumeration.
 */
void
cattle_instruction_set_value (CattleInstruction        *self,
                              CattleInstructionValue    value)
{
    gpointer enum_class;
    GEnumValue *enum_value;

    g_return_if_fail (CATTLE_IS_INSTRUCTION (self));

    /* Get the enum class for instruction values, and lookup the value.
     * If it is not present, the value is not valid */
    enum_class = g_type_class_ref (CATTLE_TYPE_INSTRUCTION_VALUE);
    enum_value = g_enum_get_value (enum_class, value);
    g_type_class_unref (enum_class);
    g_return_if_fail (enum_value != NULL);

    if (G_LIKELY (!self->priv->disposed)) {
        self->priv->value = value;
    }
}

/**
 * cattle_instruction_get_value:
 * @instruction: a #CattleInstruction
 *
 * Get the value of @instruction. See cattle_instruction_set_value().
 *
 * Return: the current value of @instruction.
 */
CattleInstructionValue
cattle_instruction_get_value (CattleInstruction *self)
{
    CattleInstructionValue value = CATTLE_INSTRUCTION_NONE;

    g_return_val_if_fail (CATTLE_IS_INSTRUCTION (self), CATTLE_INSTRUCTION_NONE);

    if (G_LIKELY (!self->priv->disposed)) {
        value = self->priv->value;
    }

    return value;
}

/**
 * cattle_instruction_set_quantity:
 * @instruction: a #CattleInstruction
 * @quantity: quantity of the instruction
 *
 * Set the number of times @instruction has to be executed.
 *
 * This value is used at runtime for faster execution, and allows to use
 * less memory for storing the code.
 */
void
cattle_instruction_set_quantity (CattleInstruction   *self,
                                 gint                 quantity)
{
    g_return_if_fail (CATTLE_IS_INSTRUCTION (self));
    g_return_if_fail ((quantity > 0) && (quantity < G_MAXINT));

    if (G_LIKELY (!self->priv->disposed)) {
        self->priv->quantity = quantity;
    }
}

/**
 * cattle_instruction_get_quantity:
 * @instruction: a #CattleInstruction
 *
 * Get the quantity of @instruction. See cattle_instruction_set_quantity().
 *
 * Return: the current quantity of @instruction.
 */
gint
cattle_instruction_get_quantity (CattleInstruction *self)
{
    gint quantity = -1;

    g_return_val_if_fail (CATTLE_IS_INSTRUCTION (self), -1);

    if (G_LIKELY (!self->priv->disposed)) {
        quantity = self->priv->quantity;
    }

    return quantity;
}

/**
 * cattle_instruction_set_next:
 * @instruction: a #CattleInstruction
 * @next: next #CattleInstruction to execute, or %NULL
 *
 * Set the next instruction to be executed.
 *
 * If @instruction has value #CATTLE_INSTRUCTION_LOOP_BEGIN, @next will be
 * executed only after the loop has returned.
 */
void
cattle_instruction_set_next (CattleInstruction   *self,
                             CattleInstruction   *next)
{
    g_return_if_fail (CATTLE_IS_INSTRUCTION (self));
    g_return_if_fail (next == NULL || CATTLE_IS_INSTRUCTION (next));

    if (G_LIKELY (!self->priv->disposed)) {

        /* Release the reference held on the previous value */
        if (G_IS_OBJECT (self->priv->next)) {
            g_object_unref (G_OBJECT (self->priv->next));
        }

        /* Set the new instruction and acquire a reference to it */
        self->priv->next = next;
        if (G_IS_OBJECT (self->priv->next)) {
            g_object_ref (G_OBJECT (self->priv->next));
        }
    }
}

/**
 * cattle_instruction_get_next:
 * @instruction: a #CattleInstruction
 *
 * Get the next instruction.
 *
 * Please note that the instruction returned might not be really the next
 * instruction to be executed: if @instruction marks the beginning of a loop
 * (i.e. its value is #CATTLE_INSTRUCTION_LOOP_BEGIN), the instruction returned
 * will be executed only after the loop has returned.
 *
 * The returned object is owned by @instruction and must not be modified or
 * freed.
 *
 * Return: the next instruction, or %NULL.
 */
CattleInstruction*
cattle_instruction_get_next (CattleInstruction *self)
{
    CattleInstruction *next = NULL;

    g_return_val_if_fail (CATTLE_IS_INSTRUCTION (self), NULL);

    if (G_LIKELY (!self->priv->disposed)) {
        next = self->priv->next;
    }

    return next;
}

/**
 * cattle_instruction_set_loop:
 * @instruction: a #CattleInstruction
 * @loop: first #CattleInstruction in the loop
 *
 * Set the instructions to be executed in the loop.
 */
void
cattle_instruction_set_loop (CattleInstruction   *self,
                             CattleInstruction   *loop)
{
    g_return_if_fail (CATTLE_IS_INSTRUCTION (self));
    g_return_if_fail (loop == NULL || CATTLE_IS_INSTRUCTION (loop));

    if (G_LIKELY (!self->priv->disposed)) {

        /* Release the reference held on the previous loop */
        if (G_IS_OBJECT (self->priv->loop)) {

            /* Releasing the first instruction in the loop causes all the
             * instructions to be disposed */
            g_object_unref (G_OBJECT (self->priv->loop));
        }

        /* Get a reference to the new instructions */
        self->priv->loop = loop;
        if (G_IS_OBJECT (self->priv->loop)) {
            g_object_ref (G_OBJECT (self->priv->loop));
        }
    }
}

/**
 * cattle_instruction_get_loop:
 * @instruction: a #CattleInstruction
 *
 * Get the first instruction of the loop.
 *
 * The returned object is owned by @instruction and must not be modified or
 * freed.
 *
 * Return: a #CattleInstruction.
 */
CattleInstruction*
cattle_instruction_get_loop (CattleInstruction *self)
{
    CattleInstruction *loop = NULL;

    g_return_val_if_fail (CATTLE_IS_INSTRUCTION (self), NULL);

    if (G_LIKELY (!self->priv->disposed)) {
        loop = self->priv->loop;
    }

    return loop;
}