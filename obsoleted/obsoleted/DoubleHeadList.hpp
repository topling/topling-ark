/* vim: set tabstop=4 : */
#ifndef DoubleHeadList_h__
#define DoubleHeadList_h__

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
# pragma once
#endif

#include <iterator>

namespace terark {

struct DoubleHeadList_NodeBase
{
	DoubleHeadList_NodeBase *prev, *next;
};
template<class T>
struct DoubleHeadList_Node : public DoubleHeadList_NodeBase
{
	T  val;
};

// this class is efficient when multi-thread producer/consumer model.
//   when used for this purpose, and only one producer one consumer,
//   it is need not mutex for modify the list, only use semaphore.
template<class T, class Alloc = std::allocator<DoubleHeadList_Node<T> > >
class DoubleHeadList : private Alloc
{
	typedef DoubleHeadList_Node<T>  Node;
	typedef DoubleHeadList_NodeBase NodeBase;

public:
	typedef			 Alloc<Node>	 allocator_type;
	typedef typename size_type		 size_type;
	typedef typename difference_type difference_type;
	typedef typename T*				 pointer;
	typedef typename const T*		 const_pointer;
	typedef typename T&				 reference;
	typedef typename const T&		 const_reference;
	typedef typename T				 value_type;

private:
	NodeBase head, tail;

	void insert_at_pos_next(NodeBase* pos, NodeBase* node) throw()
	{
		node->prev = pos;
		node->next = pos->next;
		pos->next  = node;
	}
	void insert_at_pos_prev(NodeBase* pos, NodeBase* node) throw()
	{
		node->next = pos;
		node->prev = pos->prev;
		pos->prev  = node;
	}
	void remove_node(NodeBase* x)
	{
		x->prev->next = x->next;
		x->next->prev = x->prev;
	}
	void destroy_node(NodeBase* x)
	{
		Node* p = static_cast<Node*>(x);
		Alloc::destroy(p);
		Alloc::deallocate(p);
	}
	void delete_node(NodeBase* x)
	{
		remove_node(x);
		destroy_node(x);
	}
	void remove_range(NodeBase* first, NodeBase* last)
	{
		first->prev->next = last;
		last->prev = first->prev;
	}
	Node* make_node(const T& val)
	{
		Node* node = Alloc::allocate(1);
		new(&node->val) T(val);
		return node;
	}

public:
	struct const_iterator
	{
		friend class DoubleHeadList;
	protected:
		Node* p;
		const_iterator(Node* x) : p(x) {}
	public:
		typedef std::bidirectional_iterator_tag iterator_category;
		const_iterator() {}
		const_iterator operator++() throw() { return p = p->next; }
		const_iterator operator--() throw() { return p = p->prev; }

		const_iterator operator++(int) throw()
			{ const_iterator temp = *this; p = p->next; return temp; }
		const_iterator operator--(int) throw()
			{ const_iterator temp = *this; p = p->prev; return temp; }

		const T& operator *() const throw() { return  p->val; }
		const T* operator->() const throw() { return &p->val; }
	};
	struct iterator : public const_iterator
	{
		friend class DoubleHeadList;
	protected:
		iterator(Node* x) : const_iterator(x) {}
	public:
		typedef std::bidirectional_iterator_tag iterator_category;
		iterator() {}
		iterator operator++() throw() { return p = p->next; }
		iterator operator--() throw() { return p = p->prev; }

		iterator operator++(int) throw()
			{ iterator temp = *this; p = p->next; return temp; }
		iterator operator--(int) throw()
			{ iterator temp = *this; p = p->prev; return temp; }

		T& operator *() const throw() { return  p->val; }
		T* operator->() const throw() { return &p->val; }
	};
	typedef std::reverse_iterator<iterator>		  reverse_iterator;
	typedef std::reverse_iterator<const_iterator> const_reverse_iterator;

public:
	DoubleHeadList() throw()
	{
		head.next = head.prev = &tail;
		tail.next = tail.prev = &head;
	}
	bool empty() const throw()
	{
		return (&tail != head->next);
	}
	size_type size() const throw()
	{
		size_type n = 0;
		for (NodeBase* x = head.next; x != &tail; ++x)
			++n;
		return n;
	}
	iterator begin() throw()
	{
		return iterator(static_cast<Node*>(head.next));
	}
	const_iterator begin() const throw()
	{
		return const_iterator(static_cast<Node*>(head.next));
	}
	iterator end() throw()
	{
		return iterator(static_cast<Node*>(&tail));
	}
	const_iterator end() const throw()
	{
		return const_iterator(static_cast<Node*>(&tail));
	}
	void push_front(const T& val)
	{
		insert_at_pos_next(&head, make_node(val));
	}
	void push_back(const T& val)
	{
		insert_at_pos_prev(&tail, make_node(val));
	}
	const value_type& front() const throw()
	{
		assert(!empty());
		return static_cast<Node*>(head->next)->val;
	}
	value_type& front() throw()
	{
		assert(!empty());
		return static_cast<Node*>(head->next)->val;
	}
	const value_type& back() const throw()
	{
		assert(!empty());
		return static_cast<Node*>(tail->prev)->val;
	}
	value_type& back() throw()
	{
		assert(!empty());
		return static_cast<Node*>(tail->prev)->val;
	}
	void pop_front() throw()
	{
		assert(!empty());
		delete_node(head->next);
	}
	void pop_back() throw()
	{
		assert(!empty());
		delete_node(tail->prev);
	}
	void erase(iterator iter) throw()
	{
		delete_node(iter.p);
	}
	void erase(iterator first, iterator last) throw()
	{
		remove_range(first.p, last.p);
		NodeBase* p = first.p;
		while (p != last.p)
		{
			NodeBase* next = p->next;
			destroy_node(p);
			p = next;
		}
	}
	void insert(iterator pos, const value_type& val) throw()
	{
		insert_at_pos_prev(pos.p, make_node(val));
	}
	void clear() throw()
	{
		erase(begin(), end());
	}
};

} // namespace terark

#endif // DoubleHeadList_h__
