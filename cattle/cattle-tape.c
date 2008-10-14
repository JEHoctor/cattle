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

#include "cattle-tape.h"

/**
 * SECTION:cattle-tape
 * @short_description: Infinite-length memory tape
 * @see_also: #CattleInterpreter
 *
 * #CattleTape represents an infinte-length memory tape, which is used by a
 * #CattleInterpreter to store its data. The tape is constituted by an infinte
 * amount of memory cells, each one able to store a single character.
 *
 * You can perform three operations on a tape: reading the value of the
 * current cell, writing a value on the current cell, and move the tape one
 * position to the left or to the right.
 *
 * The tape grows automatically whenever more cells are needed. You can
 * check if you are at the beginning or at the end of the tape using
 * cattle_tape_is_at_beginning() and cattle_tape_is_at_end() respectively.
 */

G_DEFINE_TYPE (CattleTape, cattle_tape, G_TYPE_OBJECT)

/**
 * CattleTape:
 *
 * Opaque data structure representing a memory tape. It should never be
 * accessed directly; use the methods below instead.
 */

#define CATTLE_TAPE_GET_PRIVATE(object) (G_TYPE_INSTANCE_GET_PRIVATE ((object), CATTLE_TYPE_TAPE, CattleTapePrivate))

struct _CattleTapePrivate {
    gboolean disposed;

    GList   *current; 	   	/* Current chunk */
    GList   *head;		   	/* First chunk */

    gint     offset;	   	/* Offset of the current cell */
	gint 	 lower_limit;	/* Offset of the first valid byte inside the first chunk */
	gint	 upper_limit;	/* Offset of the last valid byte inside the last chunk */

    GSList   *bookmarks; 	/* Bookmarks stack */
};

typedef struct _CattleTapeBookmark CattleTapeBookmark;

struct _CattleTapeBookmark {
    GList   *chunk;
    gint     offset;
};

/* Size of the tape chunk */
#define CHUNK_SIZE 128

static void
cattle_tape_init (CattleTape *self)
{
    gchar *chunk;

    self->priv = CATTLE_TAPE_GET_PRIVATE (self);

    self->priv->head = NULL;
    self->priv->current = NULL;

    /* Create the first chunk and make it the current one */
    chunk = (gchar *) g_slice_alloc0 (CHUNK_SIZE * sizeof (gchar));
    self->priv->head = g_list_append (self->priv->head, (gpointer) chunk);
    self->priv->current = self->priv->head;

	/* Set initial limits */
    self->priv->offset = 0;
	self->priv->lower_limit = 0;
	self->priv->upper_limit = 0;

	/* Initialize the bookmarks stack */
    self->priv->bookmarks = NULL;

    self->priv->disposed = FALSE;
}

static void
cattle_tape_dispose (GObject *object)
{
    CattleTape *self = CATTLE_TAPE (object);

    if (G_LIKELY (!self->priv->disposed)) {

        self->priv->disposed = TRUE;

        G_OBJECT_CLASS (cattle_tape_parent_class)->dispose (G_OBJECT (self));
    }
}

static void
chunk_free (gpointer   chunk,
            gpointer   data)
{
    g_slice_free1 (CHUNK_SIZE * sizeof (gchar), chunk);
}

static void
cattle_tape_finalize (GObject *object)
{
    CattleTape *self = CATTLE_TAPE (object);

    self->priv->current = NULL;

    g_list_foreach (self->priv->head, (GFunc) chunk_free, NULL);
    g_list_free (self->priv->head);
    self->priv->head = NULL;

    G_OBJECT_CLASS (cattle_tape_parent_class)->finalize (G_OBJECT (self));
}

static void
cattle_tape_class_init (CattleTapeClass *self)
{
    GObjectClass *object_class = G_OBJECT_CLASS (self);
    GParamSpec *pspec;

    object_class->dispose = cattle_tape_dispose;
    object_class->finalize = cattle_tape_finalize;

    g_type_class_add_private (object_class, sizeof (CattleTapePrivate));
}

/**
 * cattle_tape_new:
 *
 * Create and initialize a new memory tape.
 *
 * Return: an empty #CattleTape.
 */
CattleTape*
cattle_tape_new (void)
{
    return g_object_new (CATTLE_TYPE_TAPE, NULL);
}

/**
 * cattle_tape_set_current_value:
 * @tape: a #CattleTape
 * @value: the current cell's new value
 *
 * Set the value of the current cell.
 */
void
cattle_tape_set_current_value (CattleTape   *self,
                               gchar         value)
{
    gchar *chunk = NULL;

    g_return_if_fail (CATTLE_IS_TAPE (self));

    if (G_LIKELY (!self->priv->disposed)) {

        chunk = (char *) self->priv->current->data;
        chunk[self->priv->offset] = value;
    }
}

/**
 * cattle_tape_get_current_value:
 * @tape: a #CattleTape
 *
 * Get the value of the current cell. See cattle_tape_set_current_value().
 *
 * Return: the value of the current cell.
 */
gchar
cattle_tape_get_current_value (CattleTape *self)
{
    gchar *chunk = NULL;
    gchar value = (gchar) 0;

    g_return_val_if_fail (CATTLE_IS_TAPE (self), (gchar) 0);

    if (G_LIKELY (!self->priv->disposed)) {

        chunk = (gchar *) self->priv->current->data;
        value = chunk[self->priv->offset];
    }

    return value;
}

/**
 * cattle_tape_move_left:
 * @tape: a #CattleTape
 *
 * Move the tape one cell to the left.
 *
 * If there are no memory cells on the left of the current one, one is
 * created on the fly.
 */
void
cattle_tape_move_left (CattleTape *self)
{
    gchar *chunk;

    g_return_if_fail (CATTLE_IS_TAPE (self));

    if (G_LIKELY (!self->priv->disposed)) {

        /* We are at the beginning of the current chunk: we might need to
         * create a new chunk, if the current one is the first one */
        if (self->priv->offset == 0) {

            /* We are at the beginning of the tape */
            if (g_list_previous (self->priv->current) == NULL) {

                chunk = (gchar *) g_slice_alloc0 (CHUNK_SIZE * sizeof (gchar));
                self->priv->head = g_list_prepend (self->priv->head, chunk);

				/* Update the lower limit */
				self->priv->lower_limit = CHUNK_SIZE - 1;
            }

			/* Move to the previous chunk and update the offset */
            self->priv->current = g_list_previous (self->priv->current);
            self->priv->offset = CHUNK_SIZE - 1;
        }

		/* We are somewhere in the chunk */
        else {
            (self->priv->offset)--;

			/* If we are in the first chunk, we might need to update the
			 * lower limit */
			if (g_list_previous (self->priv->current) == NULL) {

				/* Update the lower limit */
				if (self->priv->offset < self->priv->lower_limit) {
					self->priv->lower_limit = self->priv->offset;
				}
			}
        }
    }
}

/**
 * cattle_tape_move_right:
 * @tape: a #CattleTape
 *
 * Move the tape one cell to the right.
 *
 * If there are no memory cells on the right of the current one, one is
 * created on the fly.
 */
void
cattle_tape_move_right (CattleTape *self)
{
    gchar *chunk;

    g_return_if_fail (CATTLE_IS_TAPE (self));

    if (G_LIKELY (!self->priv->disposed)) {

        /* We are at the end of the current chunk: we might need to create
         * a new chunk, if the current one is the first one */
        if (self->priv->offset == (CHUNK_SIZE - 1)) {

            /* We are at the end of the tape */
            if (g_list_next (self->priv->current) == NULL) {

                chunk = (gchar *) g_slice_alloc0 (CHUNK_SIZE * sizeof (gchar));
                self->priv->head = g_list_append (self->priv->head, chunk);

				/* Update the upper limit */
				self->priv->upper_limit = 0;
            }

			/* Move to the next chunk and update the offset */
            self->priv->current = g_list_next (self->priv->current);
            self->priv->offset = 0;
        }

        /* We are somewhere in the chunk */
        else {

            (self->priv->offset)++;

			/* If we are in the last chunk, we might need to update the
			 * upper limit */
			if (g_list_next (self->priv->current) == NULL) {

				/* Update the upper limit */
				if (self->priv->offset > self->priv->upper_limit) {
					self->priv->upper_limit = self->priv->offset;
				}
			}
        }
    }
}

/**
 * cattle_tape_is_at_beginning:
 * @tape: a #CattleTape
 *
 * Check whether or not the current cell is the first one in the tape.
 *
 * Please note that, since the tape grows automatically as more cells are
 * needed, being on the first cell doesn't mean you can't move left.
 *
 * Return: #TRUE if the current cell is the first one, #FALSE otherwise
 */
gboolean
cattle_tape_is_at_beginning (CattleTape *self)
{
    gboolean check = FALSE;

    g_return_val_if_fail (CATTLE_IS_TAPE (self), FALSE);

    if (G_LIKELY (!self->priv->disposed)) {

		/* If the current chunk is the first one and the current offset is
		 * equal to the lower limit, we are at the beginning of the tape */
        if (g_list_previous (self->priv->current) == NULL && (self->priv->offset == self->priv->lower_limit)) {
            check = TRUE;
        }
    }

    return check;
}

/**
 * cattle_tape_is_at_end:
 * @tape: a #CattleTape
 *
 * Check whether or not the current cell is the last one in the tape.
 * 
 * Please note that, since the tape grows automatically as more cells are
 * needed, being on the last cell doesn't mean you can't move right.
 *
 * Return: #TRUE if the current cell is the last one, #FALSE otherwise
 */
gboolean
cattle_tape_is_at_end (CattleTape *self)
{
    gboolean check = FALSE;

    g_return_val_if_fail (CATTLE_IS_TAPE (self), FALSE);

    if (G_LIKELY (!self->priv->disposed)) {

		/* If we are in the last chunk and the current offset is equal to the
		 * upper limit, we are in the last valid position */
        if (g_list_next (self->priv->current) == NULL && (self->priv->offset == self->priv->upper_limit)) {
            check = TRUE;
        }
    }

    return check;
}

/**
 * cattle_tape_push_bookmark:
 * @tape: a #CattleTape
 *
 * Create a bookmark to the current tape position and save it on the bookmark
 * stack.
 *
 * Since: 0.9.2
 */
void
cattle_tape_push_bookmark (CattleTape *self)
{
    CattleTapeBookmark *bookmark;

    g_return_if_fail (CATTLE_IS_TAPE (self));

	if (G_LIKELY (!self->priv->disposed)) {

		/* Create a new bookmark and store the current position */
		bookmark = g_new0 (CattleTapeBookmark, 1);
		bookmark->chunk = self->priv->current;
		bookmark->offset = self->priv->offset;

		self->priv->bookmarks = g_slist_prepend (self->priv->bookmarks, bookmark);
	}
}

/**
 * cattle_tape_pop_bookmark:
 * @tape: a #CattleTape
 *
 * Restore the previously-saved position.
 *
 * Return: #FALSE if the bookmarks stack is empty, #TRUE otherwise.
 *
 * Since: 0.9.2
 */
gboolean
cattle_tape_pop_bookmark (CattleTape *self)
{
    CattleTapeBookmark *bookmark;
	gboolean check = FALSE;

    g_return_val_if_fail (CATTLE_IS_TAPE (self), FALSE);

	if (G_LIKELY (!self->priv->disposed)) {

		if (self->priv->bookmarks != NULL) {

			/* Get the bookmark and remove it from the stack */
			bookmark = self->priv->bookmarks->data;
			self->priv->bookmarks = g_slist_remove (self->priv->bookmarks, bookmark);

			/* Restore the position */
			self->priv->current = bookmark->chunk;
			self->priv->offset = bookmark->offset;

			/* Delete the bookmark */
			g_free (bookmark);

			check = TRUE;
		}
	}

	return check;
}