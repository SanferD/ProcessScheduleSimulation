#ifndef PRIORITY_QUEUE_H
#define PRIORITY_QUEUE_H

#include <assert.h>
#include <iostream>

/* my implementation of red-black tree priority-queue. 
 * Based on Chapter 13 of Introduction to Algorithms by Cormen et al. 
 */
template <typename T, class Compare = std::less<typename T::value_type> >
class priority_queue 
{
private:
	enum Color {
		RED,
		BLACK
	};

	/* these are the nodes of the red-black tree */
	class node 
	{
	public:
		node() 
		{
			color = BLACK;
			left = right = parent = NULL;
		}

		node(T data)
		{
			this->data = data;
			color = RED;
			left = right = parent = NULL;
		}

		node *tree_minimum(node *nil, node *x) 
		{
			/* following assertion is assumed */
			assert(x != nil);

			/* keep on going left */
			while (x->left != nil)
				x = x->left;
			return x;
		}

		node *successor(node *nil) 
		{
			node *n = this;

			if (n->right != nil) // if n has a right child, find the min
				return tree_minimum(nil, n->right);
			else if (n->parent != nil) 
			{
				if (n->parent->left == n) // n is left child of parent
					return n->parent;
				else // n is right child of parent
				{
					while (n->parent->right == n)
						n = n->parent;
					return n->parent;
				}
			}
			else // n has no successor
				return nil;
		}
		
		size_t count;
		T data;
		Color color;
		node *left, *right, *parent; 
	};

	void left_rotate(node *x)
	{
		/* assumed that x is any node whose right child is not nil */
		assert(x->right != nil);

		/* y is defined as the right child of x */
		node *y = x->right;

		/* begin rotation. Assign the left subtree of y to the right of x */
		x->right = y->left;
		if (y->left != nil) // update the parent of the left subtree
			y->left->parent = x;

		/* update the parent of y */
		y->parent = x->parent;
		if (x->parent == nil) // check if x was root (and now y)
			root = y;

		/* update the child pointers of the parent */
		else if (x == x->parent->left)
			x->parent->left = y;
		else
			x->parent->right = y;

		y->left = x;
		x->parent = y;
	}

	void right_rotate(node *x)
	{
		/* assumed that x is any node whose left child is not nil */
		assert(x->left != nil);

		/* y is defined as the left child of x */
		node *y = x->left;

		/* begin rotation. Assign the right subtree of y to the left of x */
		x->left = y->right;
		if (y->right != nil) // update the parent of the right subtree
			y->right->parent = x;

		/* update the parent of y */
		y->parent = x->parent;
		if (x->parent == nil) // check if x was root (and now y)
			root = y;

		/* update the child pointers of the parent */
		else if (x == x->parent->right)
			x->parent->right = y;
		else
			x->parent->left = y;

		y->right = x;
		x->parent = y;	
	}

	void insert_fixup(node *z)
	{
		/* following assertions are assumed to be true */
		assert(z->left == nil);
		assert(z->right == nil);
		assert(z->color == RED);

		/* while the father of z is RED */
		while (z->parent->color == RED) 
		{
			node *y;
			if (z->parent == z->parent->parent->left)
			{
				y = z->parent->parent->right; /* uncle of z */
				/* if both the father and uncle of z are RED */
				if (y->color == RED)
				{
					z->parent->color = BLACK; // father->color = BLACK
					y->color = BLACK; // uncle->color = BLACK
					z->parent->parent->color = RED; // coresp. grandfather = RED
					z = z->parent->parent; /* repeat correction for grandfather */
				}
				else /* uncle color is BLACK */
				{
					if (z == z->parent->right)
					{
						z = z->parent;
						left_rotate(z);					
					}

					z->parent->color = BLACK;
					z->parent->parent->color = RED;
					right_rotate(z->parent->parent);				
				}
			}
			else
			{
				y = z->parent->parent->left;
				if (y->color == RED)
				{
					z->parent->color = BLACK;
					y->color = BLACK;
					z->parent->parent->color = RED;
					z = z->parent->parent;
				}
				else 
				{
					if (z == z->parent->left)
					{
						z = z->parent;
						right_rotate(z);					
					}

					z->parent->color = BLACK;
					z->parent->parent->color = RED;
					left_rotate(z->parent->parent);				
				}
			}
		}

		root->color = BLACK;
	}

	void insert(node *z)
	{
		/* y=predecessor; x=current; initialized for root to leaf traversal */
		node *y = nil;
		node *x = root;

		/* Traverse the tree until you find the spot to place z. 
		 * Store that location in x. y is the predecessor of x.
		 */
		while (x != nil)
		{
			y = x;
			int c = cmp(z->data, x->data);
			if (c > 0) // x > z
				x = x->left;
			else
				x = x->right;
		}

		/* is z a root node, or the left or right child of its parent? */
		z->parent = y;
		if (y == nil)
			root = z;
		else
		{
			int c = cmp(z->data, y->data);
		 	if (c > 0) // y > z
				y->left = z;
			else
				y->right = z;
		}

		z->left = nil;
		z->right = nil;

		/* fix the tree */
		insert_fixup(z);
	}

	void transplant(node *u, node *v)
	{
		if (u->parent == nil)
			root = v;
		else if (u == u->parent->left)
			u->parent->left = v;
		else
			u->parent->right = v;
		v->parent = u->parent;
	}

	node * tree_minimum(node *x) const
	{
		/* following assertion is assumed to hold true */
		assert(x != nil);

		while (x->left != nil)
			x = x->left;

		return x;
	}

	void delete_fixup(node *x)
	{
		while (x->parent != nil && x->color == BLACK)
		{
			node *w;
			if (x == x->parent->left)
			{
				w = x->parent->right;
				if (w->color == RED)
				{
					w->color = BLACK;
					x->parent->color = RED;
					left_rotate(x->parent);
					w = x->parent->right;
				}
				if (w->left->color == BLACK && w->right->color == BLACK)
				{
					w->color = RED;
					x = x->parent;
				}
				else 
				{
					if (w->right->color == BLACK)
					{
						w->left->color = BLACK;
						w->color = RED;
						right_rotate(w);
						w = x->parent->right;
					}
					w->color = x->parent->color;
					x->parent->color = BLACK;
					w->right->color = BLACK;					
					left_rotate(x->parent);
					x = root;					
				}
			}
			else
			{
				w = x->parent->left;
				if (w->color == RED)
				{
					w->color = BLACK;
					x->parent->color = RED;
					right_rotate(x->parent);
					w = x->parent->left;
				}
				if (w->right->color == BLACK && w->left->color == BLACK)
				{
					w->color = RED;
					x = x->parent;
				}
				else 
				{
					if (w->left->color == BLACK)
					{
						w->right->color = BLACK;
						w->color = RED;
						left_rotate(w);
						w = x->parent->left;
					}
					w->color = x->parent->color;
					x->parent->color = BLACK;
					w->left->color = BLACK;					
					right_rotate(x->parent);
					x = root;					
				}
				
			}
		}

		x->color = BLACK;
	}

	void delete_node(node *z)
	{
		node *x, *y = z;
		Color y_org_color = y->color;
		if (z->left == nil)
		{
			x = z->right;
			transplant(z, z->right);
		}
		else if (z->right == nil)
		{
			x = z->left;
			transplant(z, z->left);
		}
		else
		{
			y = tree_minimum(z->right);
			y_org_color = y->color;
			x = y->right;
			if (y->parent == z)
				x->parent = y;
			else
			{
				transplant(y, y->right);
				y->right = z->right;
				y->right->parent = y;
			}
			transplant(z, y);
			y->left = z->left;
			y->left->parent = y;
			y->color = z->color;
		}

		delete z;
		if (y_org_color == BLACK)
			delete_fixup(x);
	}

	
	class RBIterator 
	{
	private:
		node *current, *nil;
		RBIterator(node *n, node *c)
		{
			current = c;
			nil = n;
		}

	public:
		RBIterator()
		{
			current = NULL; nil = NULL;
		}

		bool operator!=(RBIterator const & other) const
		{
			return current != other.current;
		}

		T operator*() const
		{
			return current->data;
		}

		T* operator->() const
		{
			return &current->data; 
		}

		RBIterator& operator++()
		{
			current = current->successor(nil);
			return *this;
		}

		RBIterator operator++(int)
		{
			RBIterator t = *this;
			current = current->successor(nil);
			return t;
		}

		friend class priority_queue;
	};

public:
	priority_queue() 
	{ 
		sz = 0;
		cmp = Compare(); // function used to compare two nodes
		nil = new node(); // node with color == black
		root = nil; // root initialized to nil
		
		assert(root->color == BLACK); // property 2
		assert(nil->color == BLACK); // property 3
	}
	
	void push(const T& val)
	{
		node *n = new node(val);
		insert(n);
		sz++;

		assert(sz>0);
	}

	void pop()
	{
		if (sz)
		{
			node *n = tree_minimum(root);
			delete_node(n);
			sz--;		
		}
		assert(sz >= 0);
	}

	size_t size()
	{
		return sz;
	}

	bool empty()
	{
		return sz == 0;
	}

	const T top() const
	{
		node *n = tree_minimum(root);
		return n->data;
	}

	typedef RBIterator iterator;

	iterator end() const 
	{
		return RBIterator(nil, nil);
	}

	iterator begin() const
	{
		if (root != nil)
			return RBIterator(nil, tree_minimum(root));
		return end();
	}

	void erase(iterator position)
	{
		// assert(position!=end());

		delete_node(position.current);
		sz--;
	}

	iterator search(T data)
	{
		node *n = root;
		while(n != nil)
		{
			int c = cmp(data, n->data);
			if (c == 0)
				return RBIterator(nil, n);
			else if (c < 0) // data > n->data
				n = n->right;
			else
				n = n->left;
		}

		return end();
	}

private:
	node *root, *nil;
	Compare cmp;
	size_t sz;
};

#endif