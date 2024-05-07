// entity_component_system.h : Include file for standard system include files,
// or project specific include files.
#include <concepts>
#include <cstdint>
#include <exception>
#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <stack>
#include <stdexcept>
#include <variant>
#include <vector>

namespace ecs {
	namespace detail {

		template<typename T>
		struct to_function : public to_function<decltype(&T::operator())> {

		};

		template<typename Return, typename ... Args>
		struct to_function<Return(*)(Args...)> {
			using type = std::function<Return(Args...)>;
		};

		template<typename Class, typename Return, typename ... Args>
		struct to_function<Return(Class::*)(Args...) const> {
			using type = std::function<Return(Args...)>;
		};

		template<typename Class, typename Return, typename ... Args>
		struct to_function<Return(Class::*)(Args...)> {
			using type = std::function<Return(Args...)>;
		};

		template<typename T>
		using to_function_t = to_function<T>::type;

		template<typename T>
		to_function_t<T>::type as_function(T& lambda) {
			return static_cast<to_function_t<T>>(lambda);
		}

		template<typename ... Components>
		class Arc {
		public:

			Arc(std::size_t capacity = 4) : capacity(capacity), count(0) {
				if (capacity > 0) {
					allocate_components_tuple(components_tuple, capacity);
				}
				else {
					nullify_components_tuple(components_tuple);
				}
			}

			Arc(const Arc& other) : capacity(other.capacity), count(other.count) {
				allocate_components_tuple(components_tuple, other.capacity);
				copy_components_tuple(components_tuple, other.components_tuple, other.count);
			}

			Arc(Arc&& other) noexcept : capacity(other.capacity), count(other.count), components_tuple(std::move(other.components_tuple)) {
				nullify_components_tuple(other.components_tuple);
				other.capacity = 0;
				other.count = 0;
			}

			~Arc() {
				if (count > 0) {
					free_components_tuple(components_tuple, count);
				}
			}

			Arc& operator=(const Arc& other) {
				free_components_tuple(components_tuple, count);
				allocate_components_tuple(components_tuple, other.capacity);
				copy_components_tuple(components_tuple, other.components_tuple, other.count);
				this->capacity = other->capacity;
				this->count = other.count;
				return *this;
			}

			Arc& operator=(Arc&& other) noexcept {
				free_components_tuple(components_tuple, count);
				components_tuple = std::move(other.components_tuple);
				capacity = other.capacity;
				count = other.count;
				nullify_components_tuple(other.components_tuple);
				other.capacity = 0;
				other.count = 0;
				return *this;
			}

			void set_min_capacity(std::size_t min_capacity) {
				if (capacity >= min_capacity) {
					return;
				}

				if (capacity == 0) {
					allocate_components_tuple(components_tuple, min_capacity);
				}
				else {
					reallocate_components_tuple(components_tuple, min_capacity);
				}

				this->capacity = min_capacity;
			}

			std::tuple<const Components&...> operator[](std::size_t index) const {

#ifndef NDEBUG
				if (index >= count) {
					throw std::out_of_range("Arc::operator[] : index is out of range");
				}
#endif
				return { *(std::get<Components*>(components_tuple) + index)... };
			}

			std::tuple<const Components&...> at(std::size_t index) const {

#ifndef NDEBUG
				if (index >= count) {
					throw std::out_of_range("Arc::at : index is out of range");
				}
#endif
				return { *(std::get<Components*>(components_tuple) + index)... };
			}

			std::tuple<Components&...> operator[](std::size_t index) {

#ifndef NDEBUG
				if (index >= count) {
					throw std::out_of_range("Arc::operator[] : index is out of range");
				}
#endif

				return { *(std::get<Components*>(components_tuple) + index)... };
			}

			std::tuple<Components&...> at(std::size_t index) {

#ifndef NDEBUG
				if (index >= count) {
					throw std::out_of_range("Arc::at : index is out of range");
				}
#endif
				return { *(std::get<Components*>(components_tuple) + index)... };
			}

			std::tuple<Components&...> emplace_back() requires ((std::is_default_constructible_v<Components>) && ...) {
				if (capacity <= count) {
					set_min_capacity(capacity << 1);
				}

				default_construct_at(components_tuple, count);
				std::size_t index = count;
				++count;
				return at(index);
			}

			std::tuple<Components&...> emplace_back(Components&&... components) requires ((std::is_move_constructible_v<Components>) && ...) {
				if (capacity <= count) {
					set_min_capacity(capacity << 1);
				}

				move_construct_at(components_tuple, count, std::move(components)...);
				std::size_t index = count;
				++count;
				return at(index);
			}


			std::tuple<Components&...> push_back(const Components&... components) requires (std::is_copy_constructible_v<Components> && ...) {
				if (capacity <= count) {
					set_min_capacity(capacity << 1);
				}

				copy_construct_at(components_tuple, count, components...);
				std::size_t index = count;
				++count;
				return at(index);
			}

			std::tuple<Components&...> push_back() {
				if (capacity <= count) {
					set_min_capacity(capacity << 1);
				}

				default_construct_at(components_tuple, count);
				std::size_t index = count;
				++count;
				return at(index);
			}

			std::tuple<Components&...> back() {
				return at(count - 1);
			}


			std::tuple<Components&...> front() {
				return at(0);
			}

			void pop_back() {
				--count;
				destruct_at(components_tuple, count);
			}

			void clear() {
				((destruct_components<Components>(std::get<Components*>(components_tuple), count)), ...);
				count = 0;
				capacity = 0;
			}

			std::size_t get_count() {
				return count;
			}

			std::size_t get_capacity() {
				return capacity;
			}

			template<typename T>
			T& get_component(std::size_t index) {
				return *(std::get<T*>(components_tuple) + index);
			}

			template<typename ... Ts>
			std::tuple<Ts&...> get_components(std::size_t index) {
				return { (*(std::get<Ts*>(components_tuple) + index))... };
			}

			template<typename T>
			T* try_get_component(std::size_t index) {
				if constexpr (has_component<T>()) {
					return (std::get<T*>(components_tuple) + index);
				}
				else {
					return nullptr;
				}
			}

			template<typename T>
			static consteval bool has_component() {
				return ((std::is_same_v<T, Components>) || ...);
			}

			template<typename ... Ts>
			static consteval bool has_components() {
				return ((has_component<Ts>()) && ...);
			}

		private:


			template<typename T>
			static void destruct_components(T* components, std::size_t count) {
				if constexpr (!std::is_trivially_destructible_v<T>) {
					T* begin = components;
					T* end = components + count;

					for (T* it = begin; it != end; ++it) {
						it->~T();
					}
				}
			}

			static void nullify_components_tuple(std::tuple<Components*...>& components_tuple) {
				((std::get<Components*>(components_tuple) = nullptr), ...);
			}

			static void move_components_tuple(std::tuple<Components*...>& to, std::tuple<Components*...>& from, std::size_t count) {
				((std::move(std::get<Components*>(from), std::get<Components*>(from) + count, std::get<Components*>(to))), ...);
			}

			static void copy_components_tuple(std::tuple<Components*...>& to, std::tuple<Components*...>& from, std::size_t count) {
				((std::copy(std::get<Components*>(from), std::get<Components*>(from) + count, std::get<Components*>(to))), ...);
			}

			static void reallocate_components_tuple(std::tuple<Components*...>& components_tuple, std::size_t capacity) {
				std::tuple<Components*...> new_components_tuple = { static_cast<Components*>(std::realloc(std::get<Components*>(components_tuple), sizeof(Components) * capacity))... };
				if (((std::get<Components*>(new_components_tuple) == nullptr) || ...)) [[unlikely]] {
					((std::free(std::get<Components*>(new_components_tuple))), ...);
					throw std::bad_alloc{};
					}

				components_tuple = std::move(new_components_tuple);
			}

			static void allocate_components_tuple(std::tuple<Components*...>& components_tuple, std::size_t capacity) {
				std::tuple<Components*...> new_components_tuple = { static_cast<Components*>(std::malloc(sizeof(Components) * capacity))... };
				if (((std::get<Components*>(new_components_tuple) == nullptr) || ...)) [[unlikely]] {
					((std::free(std::get<Components*>(new_components_tuple))), ...);
					throw std::bad_alloc{};
					}

				components_tuple = std::move(new_components_tuple);
			}

			static void free_components_tuple(std::tuple<Components*...>& components_tuple, std::size_t count) {
				((destruct_components<Components>(std::get<Components*>(components_tuple), count)), ...);
				((std::free(std::get<Components*>(components_tuple))), ...);
			}

			static void copy_construct_at(std::tuple<Components*...>& components_tuple, std::size_t index, const Components&... args) {
				((new (std::get<Components*>(components_tuple) + index) Components(args)), ...);
			}

			static void move_construct_at(std::tuple<Components*...>& components_tuple, std::size_t index, Components&&... args) {
				((new (std::get<Components*>(components_tuple) + index) Components(std::move(args))), ...);
			}

			static void default_construct_at(std::tuple<Components*...>& components_tuple, std::size_t index) {
				((new (std::get<Components*>(components_tuple) + index) Components()), ...);
			}

			static void destruct_at(std::tuple<Components*...>& components_tuple, std::size_t index) {
				(((*(std::get<Components*>(components_tuple) + index)).~Components()), ...);
			}

			std::tuple<Components*...> components_tuple;
			std::size_t capacity;
			std::size_t count;
		};


	}

	struct EntityHandle {
		uint64_t archetype_index;
		uint64_t id;
	};

	template<typename ... Components>
	using EntityArchetype = detail::Arc<EntityHandle, Components...>;

	template<typename ... EntityArchetypes>
	class Scene {

	public:

		template<typename T>
		T* get_component(EntityHandle entity) {
			std::size_t component_index = 0;

			if (!find_component_index(entity, component_index)) {
				return { nullptr };
			}

			return get_component<0, T>(entity.archetype_index, component_index);
		}

		template<typename ... Ts>
		std::tuple<Ts*...> get_components(EntityHandle entity) {
			std::size_t component_index = 0;

			if (!find_component_index(entity, component_index)) {
				return { static_cast<Ts*>(nullptr)... };
			}

			return get_components<0, Ts...>(entity.archetype_index, component_index);
		}

		template<typename F>
		void for_each(F&& func) {
			for_each<0>(std::forward<F>(func), std::type_identity<detail::to_function_t<F>>{});
		}

		template<typename F>
		std::optional<EntityHandle> find_entity_if(F&& condition) {
			return find_entity_if<0>(std::forward<F>(condition), std::type_identity<detail::to_function_t<F>>{});
		}

		template<typename F>
		std::vector<EntityHandle> find_entities_where(F&& condition, std::size_t predicted_count = 4) {
			std::vector<EntityHandle> found_entities;
			found_entities.reserve(predicted_count);
			find_entities_where<0>(found_entities, std::forward<F>(condition), std::type_identity<detail::to_function_t<F>>{});
			return found_entities;
		}

		template<typename F>
		void destroy_entities_where(F&& condition) {
			destroy_entities_where<0>(std::forward<F>(condition), std::type_identity<detail::to_function_t<F>>{});
		}

		template<typename EntityArchetype>
		auto create_entity() {
			EntityArchetype& archetype = std::get<EntityArchetype>(archetypes_tuple);
			auto entity_tuple = archetype.emplace_back();
			EntityHandle& entity_handle = std::get<EntityHandle&>(entity_tuple);

			if (!recycled_ids.empty()) {
				entity_handle.id = recycled_ids.top();
				recycled_ids.pop();
				std::pair<uint64_t, std::size_t>& pair = id_index_pairs[(entity_handle.id & UINT32_MAX) - 1];
				pair.first = entity_handle.id;
				pair.second = archetype.get_count() - 1;
			}
			else {
				entity_handle.id = id_index_pairs.size() + 1;
				std::pair<uint64_t, std::size_t>& pair = id_index_pairs.emplace_back();
				pair.first = entity_handle.id;
				pair.second = archetype.get_count() - 1;
			}

			entity_handle.archetype_index = get_type_index<EntityArchetype, EntityArchetypes...>();
			return entity_tuple;
		}

		void destroy_entity(EntityHandle entity) {
			std::size_t component_index = 0;
			if (!find_component_index(entity, component_index)) {
				return;
			}

			destroy_entity<0>(entity, component_index);
		}

		void destroy_entities() {
			for (auto& pair : id_index_pairs) {
				recycled_ids.push(pair.first + (1ull << 32));
				pair.first = 0;
				pair.second = 0;
			}

			((std::get<EntityArchetypes>(archetypes_tuple).clear()), ...);
		}

	private:

		template<std::size_t ArcIter = 0>
		void destroy_entity_at_arc(ecs::EntityHandle entity, std::size_t component_index) {
			using Archetype = std::tuple_element_t<ArcIter, std::tuple<EntityArchetypes...>>;
			Archetype& archetype = std::get<ArcIter>(archetypes_tuple);

			if (component_index != archetype.get_count() - 1) {
				const uint64_t id = std::get<ecs::EntityHandle&>(archetype.back()).id;
				std::pair<uint64_t, std::size_t>& pair = id_index_pairs[(id & UINT32_MAX) - 1];
				pair.second = component_index;

				archetype.at(component_index) = std::move(archetype.back());
			}

			archetype.pop_back();

			std::pair<uint64_t, std::size_t>& pair = id_index_pairs[(entity.id & UINT32_MAX) - 1];
			pair.first = 0;
			recycled_ids.push(entity.id + (1ull << 32));
		}

		template<std::size_t ArcIter = 0>
		void destroy_entity(ecs::EntityHandle entity, std::size_t component_index) {
			if (ArcIter == entity.archetype_index) {
				using Archetype = std::tuple_element_t<ArcIter, std::tuple<EntityArchetypes...>>;
				Archetype& archetype = std::get<ArcIter>(archetypes_tuple);

				if (component_index != archetype.get_count() - 1) {
					const uint64_t id = std::get<ecs::EntityHandle&>(archetype.back()).id;
					std::pair<uint64_t, std::size_t>& pair = id_index_pairs[(id & UINT32_MAX) - 1];
					pair.second = component_index;

					archetype.at(component_index) = std::move(archetype.back());
				}

				archetype.pop_back();

				std::pair<uint64_t, std::size_t>& pair = id_index_pairs[(entity.id & UINT32_MAX) - 1];
				pair.first = 0;
				recycled_ids.push(entity.id + (1ull << 32));
			}
			else if constexpr (ArcIter < sizeof...(EntityArchetypes) - 1) {
				destroy_entity<ArcIter + 1>(entity, component_index);
			}
		}

		template<std::size_t ArcIter = 0, typename ... Ts>
		std::tuple<Ts*...> get_components(uint64_t archetype_index, std::size_t component_index) {
			if (ArcIter == archetype_index) {
				using Archetype = std::tuple_element_t<ArcIter, std::tuple<EntityArchetypes...>>;
				Archetype& archetype = std::get<ArcIter>(archetypes_tuple);
				return { archetype.template try_get_component<Ts>(component_index)... };
			}
			else if constexpr (ArcIter < sizeof...(EntityArchetypes) - 1) {
				return get_components<ArcIter + 1, Ts...>(archetype_index, component_index);
			}
			else {
				return { static_cast<Ts*>(nullptr)... };
			}
		}

		template<std::size_t ArcIter = 0, typename T>
		T* get_component(uint64_t archetype_index, std::size_t component_index) {
			if (ArcIter == archetype_index) {
				using Archetype = std::tuple_element_t<ArcIter, std::tuple<EntityArchetypes...>>;
				Archetype& archetype = std::get<Archetype>(archetypes_tuple);
				return std::addressof(archetype.template try_get_component<T>(component_index));
			}
			else if constexpr (ArcIter < sizeof...(EntityArchetypes) - 1) {
				return get_components<ArcIter + 1, T>(archetype_index, component_index);
			}
			else {
				return nullptr;
			}
		}


		template<typename T, typename Head, typename ... Tail>
		static consteval std::size_t get_type_index() {
			if constexpr (std::is_same_v<T, Head>) {
				return 0;
			}
			else {
				static_assert(sizeof...(Tail), "Cannot find archetype in scene!");
				return 1 + get_type_index<T, Tail...>();
			}
		}

		template<std::size_t ArcIter, typename Func, typename ... Args>
		void for_each(Func&& func, std::type_identity<std::function<void(Args...)>> function_identity) {
			using Archetype = std::tuple_element_t<ArcIter, std::tuple<EntityArchetypes...>>;
			Archetype& archetype = std::get<Archetype>(archetypes_tuple);
			if constexpr (Archetype::template has_components<std::decay_t<Args>...>()) {
				std::size_t count = archetype.get_count();
				for (std::size_t i = 0; i < count; ++i) {
					std::apply(func, archetype.template get_components<std::decay_t<Args>...>(i));
				}
			}

			if constexpr (ArcIter < (sizeof...(EntityArchetypes) - 1)) {
				for_each<ArcIter + 1>(std::forward<Func>(func), function_identity);
			}
		}

		template<std::size_t ArcIter, typename Func, typename ... Args>
		std::optional<EntityHandle> find_entity_if(Func&& func, std::type_identity<std::function<bool(Args...)>> function_identity) {
			using Archetype = std::tuple_element_t<ArcIter, std::tuple<EntityArchetypes...>>;
			Archetype& archetype = std::get<Archetype>(archetypes_tuple);
			if constexpr (Archetype::template has_components<std::decay_t<Args>...>()) {
				std::size_t count = archetype.get_count();
				for (std::size_t i = 0; i < count; ++i) {
					if (std::apply(func, archetype.template get_components<std::decay_t<Args>...>(i))) {
						return archetype.template get_component<EntityHandle>(i);
					}
				}
			}

			if constexpr (ArcIter < (sizeof...(EntityArchetypes) - 1)) {
				return find_entity_if<ArcIter + 1>(std::forward<Func>(func), function_identity);
			}
			else {
				return std::nullopt;
			}
		}

		template<std::size_t ArcIter, typename Func, typename ... Args>
		void find_entities_where(std::vector<EntityHandle>& found_entities, Func&& func, std::type_identity<std::function<bool(Args...)>> function_identity) {
			using Archetype = std::tuple_element_t<ArcIter, std::tuple<EntityArchetypes...>>;
			Archetype& archetype = std::get<Archetype>(archetypes_tuple);
			if constexpr (Archetype::template has_components<std::decay_t<Args>...>()) {
				std::size_t count = archetype.get_count();
				for (std::size_t i = 0; i < count; ++i) {
					if (std::apply(func, archetype.template get_components<std::decay_t<Args>...>(i))) {
						found_entities.push_back(archetype.template get_component<EntityHandle>(i));
					}
				}
			}

			if constexpr (ArcIter < (sizeof...(EntityArchetypes) - 1)) {
				return find_entities_where<ArcIter + 1>(found_entities, std::forward<Func>(func), function_identity);
			}
		}


		template<std::size_t ArcIter, typename Func, typename ... Args>
		void destroy_entities_where(Func&& func, std::type_identity<std::function<bool(Args...)>> function_identity) {
			using Archetype = std::tuple_element_t<ArcIter, std::tuple<EntityArchetypes...>>;
			Archetype& archetype = std::get<Archetype>(archetypes_tuple);
			if constexpr (Archetype::template has_components<std::decay_t<Args>...>()) {
				std::size_t count = archetype.get_count();
				if (count > 0) {
					for (std::size_t i = count - 1; i != 0; --i) {
						if (std::apply(func, archetype.template get_components<std::decay_t<Args>...>(i))) {
							destroy_entity_at_arc<ArcIter>(archetype.template get_component<EntityHandle>(i), i);
						}
					}

					if (archetype.get_count() > 0 && std::apply(func, archetype.template get_components<std::decay_t<Args>...>(0))) {
						destroy_entity_at_arc<ArcIter>(archetype.template get_component<EntityHandle>(0), 0);
					}
				}
			}

			if constexpr (ArcIter < (sizeof...(EntityArchetypes) - 1)) {
				destroy_entities_where<ArcIter + 1>(std::forward<Func>(func), function_identity);
			}
		}

		bool find_component_index(EntityHandle entity, std::size_t& out_index) const {
			std::size_t hash = (entity.id & UINT32_MAX) - 1;

			if (id_index_pairs[hash].first != entity.id) {
				return false;
			}

			out_index = id_index_pairs[hash].second;
			return true;
		}

	private:

		std::vector<std::pair<uint64_t, std::size_t>> id_index_pairs;
		std::stack<uint64_t> recycled_ids;
		std::tuple<EntityArchetypes...> archetypes_tuple;
	};
}



