/*
 This file is part of PiCo.
 PiCo is free software: you can redistribute it and/or modify
 it under the terms of the GNU Lesser General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.
 PiCo is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU Lesser General Public License for more details.
 You should have received a copy of the GNU Lesser General Public License
 along with PiCo.  If not, see <http://www.gnu.org/licenses/>.
 */
/*
 * Microbatch.hpp
 *
 *  Created on: Jan 11, 2017
 *      Author: misale
 */

#ifndef INTERNALS_TYPES_MICROBATCH_HPP_
#define INTERNALS_TYPES_MICROBATCH_HPP_

#include <cstring>

#include "Token.hpp"

#include "../ff_implementation/ff_config.hpp"

namespace pico {

class base_microbatch {
public:
	base_microbatch() :
			chunk(nullptr) {
	}

	base_microbatch(char *chunk_) :
			chunk(chunk_) {
	}

	inline bool nil() const {
		return !chunk;
	}

protected:
	char *chunk;
};

/**
 * Microbatch is the atomic storage of PiCo collections.
 * Each collection item is decorated with a token descriptor of type TokenType,
 * that also embeds the item type as:
 * DataType = TokenType::datatype.
 *
 * A Microbatch object stores a chunk of consecutive item slots.
 * Each chunk slot stores a decorated item:
 * - a TokenType token descriptor (i.e., item meta-data)
 * - a DataType collection item
 *
 * A Microbatch is used as a slot allocator.
 * Items are not initialized at Microbatch construction, they must be:
 * 1. allocated (by requesting them to the Microbatch)
 * 2. built in-place at the allocated locations
 * 3. committed
 *
 * Decorations for an item x must be built at the location:
 * token_desc(x)
 * once x has been allocated.
 *
 * This way no extra constructor calls are needed and neither DataType nor
 * TokenType are required to be DefaultConstructible.
 *
 * All allocated items must be also initialized (i.e., built) but
 * only committed items are valid collection items.
 *
 * Upon destruction, all the initialized items and descriptors are destroyed
 * and memory is freed.
 *
 * Implementation notes:
 * The entire chunk is allocated in one shot, with the purpose of reducing
 * the overall number of allocations for storing PiCo collections.
 */
template<typename TokenType>
class Microbatch: public base_microbatch {
	typedef typename TokenType::datatype DataType;

public:
	/**
	 * The empty constructor
	 */
	using base_microbatch::base_microbatch;

	/**
	 * The constructor only allocates the chunk, it does not initialize items.
	 */
	Microbatch(unsigned int slots_) :
			base_microbatch((char *) MALLOC(slots_ * slot_size)), //
			slots(slots_), allocated(0), committed(0) {
		assert(slots_);
	}

	/**
	 * The destructor destroy items and free memory.
	 */
	~Microbatch() {
		if (chunk) {
			clear();
			FREE(chunk);
		}
	}

	/**
	 * destroys all allocated items.
	 */
	void clear() {
		for (; allocated > 0; --allocated) {
			char *slot_ptr = chunk + (allocated - 1) * slot_size;
			((DataType *) (slot_ptr + desc_size))->~DataType();
			((TokenType *) (slot_ptr))->~TokenType();
		}
	}

	/**
	 * Allocates a slot from the Microbatch.
	 *
	 * Returns either:
	 * - a pointer to the first free slot for a data item
	 * - nullptr if the chunk is full
	 */
	inline DataType *allocate() {
		if (!full())
			return (DataType *) (chunk + (allocated++ * slot_size) + desc_size);
		return nullptr;
	}

	/**
	 * Commits the last allocated item.
	 */
	inline void commit() {
		assert(committed < slots);
		++committed;
	}

	/**
	 * Return a pointer to the token descriptor for a data slot.
	 */
	static TokenType *token_desc(DataType *data_slot) {
		return (TokenType *) (((char *) (data_slot)) - desc_size);
	}

	inline bool full() const {
		return allocated == slots;
	}

	inline bool empty() const {
		return allocated == 0;
	}

	/*
	 * Microbatch iterator over committed items.
	 */
	class iterator: public std::iterator<std::input_iterator_tag, DataType> {
		char* p;
	public:
		iterator(char *x) :
				p(x) {
		}
		iterator& operator++() {
			p += slot_size;
			return *this;
		}
		bool operator==(const iterator& rhs) {
			return p == rhs.p;
		}
		bool operator!=(const iterator& rhs) {
			return p != rhs.p;
		}
		DataType& operator*() {
			return *(DataType *) (p + desc_size);
		}
	};

	iterator begin() {
		return iterator(chunk);
	}

	iterator end() {
		return iterator(chunk + committed * slot_size);
	}

private:
	static const size_t desc_size = sizeof(TokenType);
	static const size_t slot_size = desc_size + sizeof(DataType);
	const unsigned int slots;
	unsigned int allocated, committed;
};

template<typename T>
class mb_wrapped: public Microbatch<Token<T *>> {
public:
	using Microbatch<Token<T *>>::Microbatch;
	mb_wrapped(T *ptr) :
			Microbatch<Token<T *>>(1) {
		*(this->allocate()) = ptr;
		this->commit();
	}

	T *get() {
		return *this->begin();
	}
};

} /* namespace pico */

#endif /* INTERNALS_TYPES_MICROBATCH_HPP_ */
