/*
 * Copyright (C) 2010 Alex Smith
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * @file
 * @brief		Window list class.
 */

#ifndef __WINDOWLIST_H
#define __WINDOWLIST_H

#include <iterator>
#include <list>
#include <map>
#include <stdint.h>

class ServerWindow;

/** Class managing the order of windows. */
class WindowList {
	/** Internal list type. */
	typedef std::list<ServerWindow *> List;

	/** Type of the map of levels to window lists. */
	typedef std::map<uint32_t, List> Map;
public:
	/** List iterator type. */
	class Iterator : public std::iterator<std::bidirectional_iterator_tag, ServerWindow *const> {
		friend class WindowList;
	public:
		Iterator(const Iterator &other) :
			m_map(other.m_map), m_map_iter(other.m_map_iter),
			m_list_iter(other.m_list_iter)
		{}

		Iterator &operator ++();
		Iterator &operator --();
		bool operator ==(const Iterator &other);
		bool operator !=(const Iterator &other);

		Iterator operator ++(int) {
			Iterator tmp(*this);
			operator ++();
			return tmp;
		}

		Iterator operator --(int) {
			Iterator tmp(*this);
			operator --();
			return tmp;
		}

		ServerWindow *const &operator *() {
			return *m_list_iter;
		}
	private:
		Iterator(const Map &map, Map::const_iterator iter);

		const Map &m_map;
		Map::const_iterator m_map_iter;
		List::const_iterator m_list_iter;
	};

	/** Reverse iterator type. */
	typedef std::reverse_iterator<Iterator> ReverseIterator;

	WindowList();

	void Insert(ServerWindow *window);
	void Remove(ServerWindow *window);
	bool MoveToFront(ServerWindow *window);

	/** Get the list head.
	 * @return		Iterator to first window in the list. */
	Iterator Begin() const { return Iterator(m_windows, m_windows.begin()); }

	/** Get the list end.
	 * @return		Iterator pointing after last window in the list. */
	Iterator End() const { return Iterator(m_windows, m_windows.end()); }

	/** Get the reverse list head.
	 * @return		Iterator to last window in the list. */
	ReverseIterator ReverseBegin() const { return ReverseIterator(End()); }

	/** Get the reverse list end.
	 * @return		Iterator pointing after first window in the list. */
	ReverseIterator ReverseEnd() const { return ReverseIterator(Begin()); }
private:
	List &ListForWindow(ServerWindow *window);

	Map m_windows;			/**< Map of levels to window lists. */
};

#endif /* __WINDOWLIST_H */
