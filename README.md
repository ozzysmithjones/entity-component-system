# Entity-component-system
Header only entity component system. 

# Explanation
An Entity-Component-System is a data-orientated approach of organising entities within games. The idea is to layout entities (game characters, walls, objects) in a cache-friendly way so that they can be processed efficiently by the CPU. ECS uses three concepts to structure entities in an efficient way:

- Components: Components are structures that store data.
- Entities: Entities in ECS are just identifiers used to associate the components that make up a single instance of an object. These identifiers may be indices or 64 bit values.
- Systems: Systems are functions or behaviours that process a collection of components to perform logic.

Note that the behaviour and state are separate. Generally, components only contain state and they are stored in contiguous arrays for efficiency. Systems iterate through the components and apply transformations to the state to implement certain behaviours - like rendering or physics calculations. Storing the components this way minimizes the overall *cache misses* that might occur, which is often a bottleneck when processing a large amount of game characters.

# API

This Entity-Component-System uses two primary types: *Scene* and *EntityArchetype*. You use *EntityArchetype* to declare the entity kinds that you want in your game and their components:
Note that "components" in this implementation are just C++ structs.

```C++
using Human = ecs::EntityArchetype<Transform, Mesh, Collider, Health>;
using Goblin = ecs::EntityArchetype<Transform, Mesh, Health>;
```

You use Scene to create a game *level* or *world* that contains these entity kinds:

```C++
ecs::Scene<Human, Goblin> first_level;
```

To implement system like behaviour, you can call for_each(). Which iterates over the components in the scene and performs some behaviour with them: 

```C++
ecs::Scene<Human, Goblin> first_level;
first_level.for_each([](const Transform& transform, const Mesh& mesh) { render(transform, mesh); });
```
To create or destroy entities you can use create_entity() or destroy_entity():

```C++
ecs::Scene<Human, Goblin> first_level;
auto [identifier, transform, mesh, health] = first_level.create_entity<Goblin>();
first_level.destroy_entity(identifier);
```

To find particular components, you can use the identifiers to search for them:

```C++
ecs::Scene<Human, Goblin> first_level;
auto [identifier, transform, mesh, health] = first_level.create_entity<Goblin>();
Transform* transform = first_level.get_component<Transform>(identifier);
```

# Implementation Details
The reason this ECS requires that you declare the entity kinds upfront is so that computation can be made at compile-time instead of run-time. The implementation is organised into archetypes similar to Unity DOTS. See the source code for more details. 
