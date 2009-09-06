/*
 * plist-utils.c
 *
 * Copyright (C) 2009 Martin Szulecki <opensuse@sukimashita.com>
 * 
 * The code contained in this file is free software; you can redistribute
 * it and/or modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either version
 * 2.1 of the License, or (at your option) any later version.
 *  
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *  
 * You should have received a copy of the GNU Lesser General Public
 * License along with this code; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 * 
 */

#include <stdlib.h>

#include "plist-utils.h"

int plist_node_get_item_count(plist_t node) {
	plist_t child;
	plist_type type = PLIST_KEY;
	plist_type child_type;
	int count = 0;

	type = plist_get_node_type(node);

	child = plist_get_first_child(node);
	if (child == NULL)
		return count;

	for(child; child != NULL; child = plist_get_next_sibling(child)) {
		if (type == PLIST_ARRAY) {
			child_type = plist_get_node_type(child);
			if ((child_type != PLIST_DICT) && (child_type != PLIST_ARRAY))
				count++;
		}
		else if (plist_get_node_type(child) == PLIST_KEY) {
			count++;
		}
	}

	return count;
}

int plist_item_index(plist_t node) {
	plist_t parent;
	plist_t child;
	int count = 0;

	parent = plist_get_parent(node);

	if (parent == NULL || node == NULL) {
		return -1;
	}

	child = plist_get_first_child(parent);
	while (child && child != node) {
		count ++;
		child = plist_get_next_sibling(child);
	}

	return count;
}

