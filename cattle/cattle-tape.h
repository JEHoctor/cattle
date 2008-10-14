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

#ifndef __CATTLE_TAPE_H__
#define __CATTLE_TAPE_H__

#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define CATTLE_TYPE_TAPE                (cattle_tape_get_type ())
#define CATTLE_TAPE(object)             (G_TYPE_CHECK_INSTANCE_CAST ((object), CATTLE_TYPE_TAPE, CattleTape))
#define CATTLE_TAPE_CLASS(klass)        (G_TYPE_CHECK_CLASS_CAST ((klass), CATTLE_TYPE_TAPE, CattleTapeClass))
#define CATTLE_IS_TAPE(object)          (G_TYPE_CHECK_INSTANCE_TYPE ((object), CATTLE_TYPE_TAPE))
#define CATTLE_IS_TAPE_CLASS(klass)     (G_TYPE_CHECK_CLASS_TYPE ((klass), CATTLE_TYPE_TAPE))
#define CATTLE_TAPE_GET_CLASS(object)   (G_TYPE_INSTANCE_GET_CLASS ((object), CATTLE_TYPE_TAPE, CattleTapeClass))

typedef struct _CattleTape          CattleTape;
typedef struct _CattleTapeClass     CattleTapeClass;
typedef struct _CattleTapePrivate   CattleTapePrivate;

struct _CattleTape {
    GObject parent;
    CattleTapePrivate *priv;
};

struct _CattleTapeClass {
    GObjectClass parent;
};

CattleTape*   cattle_tape_new                 (void);
void          cattle_tape_set_current_value   (CattleTape   *tape,
                                               gchar         value);
gchar         cattle_tape_get_current_value   (CattleTape   *tape);
void          cattle_tape_move_left           (CattleTape   *tape);
void          cattle_tape_move_right          (CattleTape   *tape);
gboolean      cattle_tape_is_at_beginning     (CattleTape   *tape);
gboolean      cattle_tape_is_at_end           (CattleTape   *tape);
void          cattle_tape_push_bookmark       (CattleTape   *tape);
gboolean      cattle_tape_pop_bookmark        (CattleTape   *tape);

GType         cattle_tape_get_type            (void) G_GNUC_CONST;

G_END_DECLS

#endif /* __CATTLE_TAPE_H__ */