#include "rb_tree.hpp"

void _Terark_Rb_tree_base_iterator::increment()
{
	if (node->right != 0) {
		node = node->right;
		while (node->left != 0)
			node = node->left;
	}
	else {
		_Terark_Rb_tree_node_base* y = node->getparent();
		while (node == y->right) {
			node = y;
			y = y->getparent();
		}
		if (node->right != y)
			node = y;
	}
}

void _Terark_Rb_tree_base_iterator::decrement()
{
	if (node->isRed() &&
		node->getparent()->getparent() == node)
		node = node->right;
	else if (node->left != 0) {
		_Terark_Rb_tree_node_base* y = node->left;
		while (y->right != 0)
			y = y->right;
		node = y;
	}
	else {
		_Terark_Rb_tree_node_base* y = node->getparent();
		while (node == y->left) {
			node = y;
			y = y->getparent();
		}
		node = y;
	}
}

void _Terark_Rb_tree_rotate_left(_Terark_Rb_tree_node_base* x, _Terark_Rb_tree_node_base*& root)
{
	_Terark_Rb_tree_node_base* y = x->right;
	x->right = y->left;
	if (y->left !=0)
		y->left->setparent(x);
	y->setparent(x->getparent());

	if (x == root)
		root = y;
	else if (x == x->getparent()->left)
		x->getparent()->left = y;
	else
		x->getparent()->right = y;
	y->left = x;
	x->setparent(y);
}

void _Terark_Rb_tree_rotate_right(_Terark_Rb_tree_node_base* x, _Terark_Rb_tree_node_base*& root)
{
	_Terark_Rb_tree_node_base* y = x->left;
	x->left = y->right;
	if (y->right != 0)
		y->right->setparent(x);
	y->setparent(x->getparent());

	if (x == root)
		root = y;
	else if (x == x->getparent()->right)
		x->getparent()->right = y;
	else
		x->getparent()->left = y;
	y->right = x;
	x->setparent(y);
}

void _Terark_Rb_tree_rebalance(_Terark_Rb_tree_node_base* x, _Terark_Rb_tree_node_base*& root)
{
	x->setRed();
	while (x != root && x->getparent()->isRed())
	{
		if (x->getparent() == x->getparent()->getparent()->left)
		{
			_Terark_Rb_tree_node_base* y = x->getparent()->getparent()->right;
			if (y && y->isRed()) {
				x->getparent()->setblack();
				y->setblack();
				x->getparent()->getparent()->setRed();
				x = x->getparent()->getparent();
			}
			else {
				if (x == x->getparent()->right) {
					x = x->getparent();
					_Terark_Rb_tree_rotate_left(x, root);
				}
				x->getparent()->setblack();
				x->getparent()->getparent()->setRed();
				_Terark_Rb_tree_rotate_right(x->getparent()->getparent(), root);
			}
		}
		else {
			_Terark_Rb_tree_node_base* y = x->getparent()->getparent()->left;
			if (y && y->isRed()) {
				x->getparent()->setblack();
				y->setblack();
				x->getparent()->getparent()->setRed();
				x = x->getparent()->getparent();
			}
			else {
				if (x == x->getparent()->left) {
					x = x->getparent();
					_Terark_Rb_tree_rotate_right(x, root);
				}
				x->getparent()->setblack();
				x->getparent()->getparent()->setRed();
				_Terark_Rb_tree_rotate_left(x->getparent()->getparent(), root);
			}
		}
	}
	root->setblack();
}

_Terark_Rb_tree_node_base*
_Terark_Rb_tree_rebalance_for_erase(_Terark_Rb_tree_node_base* z,
							 _Terark_Rb_tree_node_base*& root,
							 _Terark_Rb_tree_node_base*& leftmost,
							 _Terark_Rb_tree_node_base*& rightmost)
{
	_Terark_Rb_tree_node_base* y = z;
	_Terark_Rb_tree_node_base* x = 0;
	_Terark_Rb_tree_node_base* x_parent = 0;
	if (y->left == 0)     // z has at most one non-null child. y == z.
		x = y->right;     // x might be null.
	else {
		if (y->right == 0)  // z has exactly one non-null child. y == z.
			x = y->left;    // x is not null.
		else {                   // z has two non-null children.  Set y to
			y = y->right;   //   z's successor.  x might be null.
			while (y->left != 0)
				y = y->left;
			x = y->right;
		}
	}
	if (y != z) {          // relink y in place of z.  y is z's successor
		z->left->setparent(y);
		y->left = z->left;
		if (y != z->right) {
			x_parent = y->getparent();
			if (x) x->setparent(y->getparent());
			y->getparent()->left = x;      // y must be a child of left
			y->right = z->right;
			z->right->setparent(y);
		}
		else
			x_parent = y;
		if (root == z)
			root = y;
		else if (z->getparent()->left == z)
			z->getparent()->left = y;
		else
			z->getparent()->right = y;
		y->setparent(z->getparent());
		y->swapcolor(z);
		y = z;
		// y now points to node to be actually deleted
	}
	else {                        // y == z
		x_parent = y->getparent();
		if (x) x->setparent(y->getparent());
		if (root == z)
			root = x;
		else
			if (z->getparent()->left == z)
				z->getparent()->left = x;
			else
				z->getparent()->right = x;
		if (leftmost == z)
			if (z->right == 0)        // z->left must be null also
				leftmost = z->getparent();
		// makes leftmost == header if z == root
			else
				leftmost = _Terark_Rb_tree_node_base::_S_minimum(x);
		if (rightmost == z)
			if (z->left == 0)         // z->right must be null also
				rightmost = z->getparent();
		// makes rightmost == header if z == root
			else                      // x == z->left
				rightmost = _Terark_Rb_tree_node_base::_S_maximum(x);
	}
	if (!y->isRed())
	{
		while (x != root && (x == 0 || x->isblack()))
		{
			if (x == x_parent->left)
			{
				_Terark_Rb_tree_node_base* w = x_parent->right;
				if (w->isRed()) {
					w->setblack();
					x_parent->setRed();
					_Terark_Rb_tree_rotate_left(x_parent, root);
					w = x_parent->right;
				}
				if ((w->left == 0 || w->left->isblack()) &&
					(w->right == 0 || w->right->isblack()))
				{
					w->setRed();
					x = x_parent;
					x_parent = x_parent->getparent();
				} else {
					if (w->right == 0 || w->right->isblack())
					{
						if (w->left)
							w->left->setblack();
						w->setRed();
						_Terark_Rb_tree_rotate_right(w, root);
						w = x_parent->right;
					}
					w->copycolor(x_parent);
					x_parent->setblack();
					if (w->right) w->right->setblack();
					_Terark_Rb_tree_rotate_left(x_parent, root);
					break;
				}
			} else {                  // same as above, with right <-> left.
				_Terark_Rb_tree_node_base* w = x_parent->left;
				if (w->isRed()) {
					w->setblack();
					x_parent->setRed();
					_Terark_Rb_tree_rotate_right(x_parent, root);
					w = x_parent->left;
				}
				if ((w->right == 0 || w->right->isblack()) &&
					(w->left == 0  || w->left->isblack()))
				{
						w->setRed();
						x = x_parent;
						x_parent = x_parent->getparent();
				} else {
					if (w->left == 0 ||	w->left->isblack())
					{
							if (w->right) w->right->setblack();
							w->setRed();
							_Terark_Rb_tree_rotate_left(w, root);
							w = x_parent->left;
					}
					w->copycolor(x_parent);
					x_parent->setblack();
					if (w->left) w->left->setblack();
					_Terark_Rb_tree_rotate_right(x_parent, root);
					break;
				}
			}
			if (x) x->setblack();
		}
	}
	return y;
}
