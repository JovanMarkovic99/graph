#pragma once
// For std::...
#include <functional>
#include <stdexcept>
#include <unordered_map>

namespace jvn
{

	template <class V, bool Directed = false, bool Weighted = false,
		class VerEq = std::equal_to<V>, class Alloc = std::allocator<V>>
		class Graph
	{
	public:
		using vertex_type					= V;
		using edge_type						= std::conditional_t<Weighted,
											std::tuple<vertex_type, vertex_type, int>,
											std::tuple<vertex_type, vertex_type>>;
		using size_type						= size_t;
		using vertex_equal					= VerEq;
		using allocator_type				= Alloc;
	private:
		//	Defining Node type traits ------------------------------------------

		//	Forward declare VertexNode
		struct VertexNode;

		template <bool W>
		struct EdgeNodeConditional
		{
			EdgeNodeConditional(VertexNode* v)
				:vertex_node(v)
				{}
			int weight						= 0;
			VertexNode* vertex_node			= nullptr;
			EdgeNodeConditional* next		= nullptr;
		};

		template <>
		struct EdgeNodeConditional<false>
		{
			EdgeNodeConditional(VertexNode* v)
				:vertex_node(v)
				{}
			VertexNode* vertex_node			= nullptr;
			EdgeNodeConditional* next		= nullptr;
		};

		using EdgeNode = EdgeNodeConditional<Weighted>;

		struct VertexNode
		{
			VertexNode(const vertex_type& v)
				:vertex(v) 
				{}
			VertexNode(vertex_type&& v)
				:vertex(std::move(v)) 
				{}
			vertex_type vertex;
			EdgeNode* edge_list				= nullptr;
			VertexNode* next				= nullptr;
		};

		using vertex_node_allocator_type	= typename allocator_type::template rebind<VertexNode>::other;
		using edge_node_allocator_type		= typename allocator_type::template rebind<EdgeNode>::other;

		//	------------------------------------------ Defining Node type traits
	private:
		//	Defining Iter type traits ------------------------------------------

		// Forward declare VertexIter
		class VertexIter;

		class EdgeIter
		{
		public:
			~EdgeIter()								= default;
			EdgeIter(const EdgeIter&)				= default;
			EdgeIter& operator=(const EdgeIter&)	= default;

			friend constexpr bool operator==(const EdgeIter& lhs, const EdgeIter& rhs) noexcept { return lhs.m_edge_node == rhs.m_edge_node; }
			friend constexpr bool operator!=(const EdgeIter& lhs, const EdgeIter& rhs) noexcept { return !(lhs == rhs); }
			EdgeIter& operator++()
			{
				if (m_edge_node == nullptr)
					throw std::runtime_error("End of iteration reached");
				m_edge_node = m_edge_node->next;
				return *this;
			}
			edge_type operator*()
			{
				if (m_edge_node == nullptr)
					throw std::runtime_error("Invalid iterator");
				return getTuple<Weighted>();
			}

			constexpr VertexIter getStartVertex() const noexcept { return VertexIter(m_vertex_node); }

			friend class Graph;
		private:
			EdgeIter(VertexNode* vertex_node, EdgeNode* edge_node) noexcept
				:m_vertex_node(vertex_node),
				m_edge_node(edge_node)
			{}

			// Two templates for weighted and non-weighted
			template <bool W>
			constexpr edge_type getTuple() { return std::make_tuple(m_vertex_node->vertex, m_edge_node->vertex_node->vertex, m_edge_node->weight); }
			template <>
			constexpr edge_type getTuple<false>() { return std::make_tuple(m_vertex_node->vertex, m_edge_node->vertex_node->vertex); }

			VertexNode* m_vertex_node;
			EdgeNode* m_edge_node;
		};

		class VertexIter
		{
		public:
			~VertexIter()								= default;
			VertexIter(const VertexIter&)				= default;
			VertexIter& operator=(const VertexIter&)	= default;

			friend constexpr bool operator==(const VertexIter& lhs, const VertexIter& rhs) noexcept { return lhs.m_vertex_node == rhs.m_vertex_node; }
			friend constexpr bool operator!=(const VertexIter& lhs, const VertexIter& rhs) noexcept { return !(lhs == rhs); }
			VertexIter& operator++()
			{
				if (m_vertex_node == nullptr)
					throw std::runtime_error("End of iteration reached");
				m_vertex_node = m_vertex_node->next;
				return *this;
			}
			vertex_type* operator->() 
			{
				if (m_vertex_node == nullptr)
					throw std::runtime_error("Invalid iterator");
				return &(m_vertex_node->vertex); 
			}
			vertex_type& operator*() 
			{ 
				if (m_vertex_node == nullptr)
					throw std::runtime_error("Invalid iterator");
				return m_vertex_node->vertex; 
			}

			EdgeIter getEdges() noexcept { return EdgeIter(m_vertex_node, m_vertex_node->edge_list); }

			friend class Graph;
		private:
			VertexIter(VertexNode* vertex_node) noexcept
				:m_vertex_node(vertex_node)
				{}

			VertexNode* m_vertex_node;
		};

		//	------------------------------------------ Defining Node type traits

	public:
		using vertex_iterator				= VertexIter;
		using edge_iterator					= EdgeIter;

	// ------------------------------------------- GRAPH MAIN LOGIC -------------------------------------------
	public:
		Graph() 
			:m_vertex_node_list(nullptr),
			m_size(0)
			{};
		Graph(std::initializer_list<edge_type> list): Graph()
		{
			for (auto i = list.begin(); i != list.end(); ++i)
				addEdge(*i);
		}
		Graph(const Graph& g) : Graph() { operator=(g); }
		Graph(Graph&& g) : Graph() { swap(g, *this); }
		Graph& operator=(const Graph& g)
		{
			destroyGraph();
			for (auto v = g.begin(); v != g.end(); ++v)
				for (auto e = v.getEdges(); e != g.edge_end(); ++e)
					addEdge(*e);
			return *this;
		}
		Graph& operator=(Graph&& g) { swap(g, *this); return *this; }
		~Graph() { destroyGraph(); }

		template <class Ty, std::enable_if_t<std::is_same<std::decay_t<Ty>, vertex_type>::value, int> = 0>
		std::tuple<vertex_iterator, bool> addVertex(Ty&& vertex)
		{
			if (m_vertex_node_list == nullptr)
			{
				m_vertex_node_list = m_vertex_node_allocator.allocate(1);
				m_vertex_node_allocator.construct(m_vertex_node_list, std::forward<Ty>(vertex));
				++m_size;
				return std::make_tuple(vertex_iterator(m_vertex_node_list), true);
			}

			auto search = findVertexHelper(vertex);

			if (vertex_equal{}(search->vertex, vertex))
				return std::make_tuple(vertex_iterator(search), false);

			search->next = m_vertex_node_allocator.allocate(1);
			m_vertex_node_allocator.construct(search->next, std::forward<Ty>(vertex));
			++m_size;
			return std::make_tuple(vertex_iterator(search->next), true);
		}

		void addVertex(std::initializer_list<vertex_type> list)
		{
			for (auto i = list.begin(); i != list.end(); ++i)
				addVertex(*i);
		}

		std::tuple<edge_iterator, bool> addEdge(const edge_type& edge)
		{ return addEdgeCaller<Directed>(edge); }

		void addEdge(std::initializer_list<edge_type> list)
		{
			for (auto i = list.begin(); i != list.end(); ++i)
				addEdge(*i);
		}

		vertex_iterator findVertex(const vertex_type& vertex) const
		{
			auto search = findVertexHelper(vertex);
			if (search == nullptr || !vertex_equal{}(search->vertex, vertex))
				return end();
			return vertex_iterator(search);
		}

		edge_iterator findEdge(const edge_type& edge) const
		{
			auto from = findVertex(std::get<0>(edge)).m_vertex_node;
			auto to = findVertex(std::get<1>(edge)).m_vertex_node;

			auto edge_search = findEdgeHelper(from, to);
			if (edge_search != nullptr && edge_search->vertex_node != to)
				return edge_end();
			return edge_iterator(from, edge_search);
		}

		constexpr vertex_iterator begin() const noexcept { return vertex_iterator(m_vertex_node_list); }
		static constexpr vertex_iterator end() noexcept { return vertex_iterator(nullptr); }
		static constexpr edge_iterator edge_end() noexcept { return edge_iterator(nullptr, nullptr); }

		friend void swap(const Graph& lhs, const Graph& rhs)
		{
			// Enable ADL
			using std::swap;
			swap(lhs.m_vertex_node_list, rhs.m_vertex_node_list);
			swap(lhs.m_vertex_node_allocator, rhs.vertex_node_allocator);
			swap(lhs.m_edge_node_allocator, rhs.m_edge_node_allocator);
			swap(lhs.m_size, rhs.m_size);
		}
	private:
		VertexNode* m_vertex_node_list;
		vertex_node_allocator_type m_vertex_node_allocator;
		edge_node_allocator_type m_edge_node_allocator;
		size_type m_size;

		// Edge Helpers ----------------

		std::tuple<edge_iterator, bool> addEdgeHelper(VertexNode* from, VertexNode* to, int weight = 0)
		{
			if (from->edge_list == nullptr)
			{
				from->edge_list = m_edge_node_allocator.allocate(1);
				m_edge_node_allocator.construct(from->edge_list, EdgeNode(to));
				setEdgeWeight<Weighted>(from->edge_list, weight);
				return std::make_tuple(edge_iterator(from, from->edge_list), true);
			}

			auto edge_search = findEdgeHelper(from, to);

			if (edge_search->vertex_node == to)
				return std::make_tuple(edge_iterator(from, edge_search), false);

			edge_search->next = m_edge_node_allocator.allocate(1);
			m_edge_node_allocator.construct(edge_search->next, EdgeNode(to));
			setEdgeWeight<Weighted>(edge_search->next, weight);
			return std::make_tuple(edge_iterator(from, edge_search->next), true);
		}

		// Specialized functions for directed and non-directed
		template <bool W>
		std::tuple<edge_iterator, bool> addEdgeCaller(const edge_type& edge)
		{
			auto vertex_from_node = std::get<0>(addVertex(std::get<0>(edge))).m_vertex_node;
			auto vertex_to_node = std::get<0>(addVertex(std::get<1>(edge))).m_vertex_node;

			addEdgeHelper(vertex_to_node, vertex_from_node,
				std::intptr_t(std::get<std::tuple_size<edge_type>::value - 1>(edge)));
			return addEdgeHelper(vertex_from_node, vertex_to_node,
				std::intptr_t(std::get<std::tuple_size<edge_type>::value - 1>(edge))); 
		}
		template <>
		std::tuple<edge_iterator, bool> addEdgeCaller<true>(const edge_type& edge)
		{
			auto vertex_from_node = std::get<0>(addVertex(std::get<0>(edge))).m_vertex_node;
			auto vertex_to_node = std::get<0>(addVertex(std::get<1>(edge))).m_vertex_node;

			return addEdgeHelper(vertex_from_node, vertex_to_node,
				std::intptr_t(std::get<std::tuple_size<edge_type>::value - 1>(edge)));
		}

	
		// Specialized functions for weighted and unweighted	
		template <bool W>
		constexpr void setEdgeWeight(EdgeNode* edge, int weight) const
		{ edge->weight = weight; }
		template <>
		constexpr void setEdgeWeight<false>(EdgeNode* edge, int weight) const
		{}

		// Returns the edge with the vertex ot the last edge in the list or nullptr if the edge list is empty
		EdgeNode* findEdgeHelper(VertexNode* from, VertexNode* to) const
		{
			if (from == nullptr || to == nullptr)
				return nullptr;
			
			auto search = from->edge_list;
			while (search != nullptr && search->next != nullptr && search->vertex_node != to)
				search = search->next;
			return search;
		}

		// ---------------- Edge Helpers

		// Returns the node with the vertex or the last node in the list or nullptr if the list is empty
		VertexNode* findVertexHelper(const vertex_type& vertex) const
		{
			if (m_vertex_node_list == nullptr)
				return nullptr;

			auto search = m_vertex_node_list;
			while (search->next != nullptr && !vertex_equal{}(vertex, search->vertex))
				search = search->next;
			return search;
		}

		void destroyGraph()
		{
			auto search = m_vertex_node_list;
			while (search != nullptr)
			{
				auto next = search->next;

				// Deallocate edge list
				auto edge_search = search->edge_list;
				while (edge_search != nullptr)
				{
					auto edge_next = edge_search->next;
					m_edge_node_allocator.deallocate(edge_search, 1);
					edge_search = edge_next;
				}

				m_vertex_node_allocator.destroy(search);
				m_vertex_node_allocator.deallocate(search, 1);
				search = next;
			}
			m_size = 0;
			m_vertex_node_list = nullptr;
		}
	};

}	// namespace jvn