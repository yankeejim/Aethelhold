# Aethelhold

A high-fantasy city-builder for PC in Unreal Engine 5. Team: Jim (designer, owner) and Claude.
The agreed concept is in `docs/concept.md`. Read it before designing or building anything.
Detailed designs: `docs/resources.md` (materials, workshops, hauling, spoilage, skills).

## Settled decisions (don't change without asking Jim)

- Scale like Songs of Syx: from a handful of founders to a city of thousands. Endless sandbox.
- High fantasy. The player chooses the city's race at the start; other races live on the map.
- The player is an unseen manager, as hands-on as Songs of Syx, Dwarf Fortress and RimWorld:
  building and materials, farming, production, work, housing, defense.
- The city has a ruler the player doesn't control. Notables (ruler, heirs, nobles, guild masters,
  priests, generals) have AI minds and drive politics and succession. The player influences them
  only indirectly, mostly through what gets built.
- Drama: interpersonal conflict, the city itself (fire, plague, hunger, unrest), rival kingdoms,
  and occasional monsters.
- In the judgment hall the player is the ruler's unseen counsel ("the Steward"): the ruler usually
  follows the verdict but may overrule it.
- War: the player commands defense of the city; the general leads campaigns abroad.
- Look: like Manor Lords (grounded, natural, detailed). The first test uses humans only.
- Two layers of people: the crowd (thousands) runs on simple coded rules with no AI model, so it
  stays fast; a few dozen notables have AI minds.
- The notables think with a local model on the player's PC (Ollama; qwen2.5 7b by default,
  14b as the option for strong PCs).
- First playable milestone: "the founding" (seven founders, the first winter; see `docs/concept.md`).

## Architecture rules

- Unreal Engine 5.8, C++. Simulation and graphics live in the same project; crowds use Mass.
- The AI only shapes words and picks from a fixed list of actions. The simulation alone decides
  what an action does, and never reads the AI's prose back to make decisions.
- Notables know only what is grounded in real events in the simulation: what they saw
  themselves, or heard through news and gossip. Nothing they say or remember may come from
  information they couldn't have had.
- Saves keep the city as it is, but the game doesn't need to replay identically after loading.
  Strict determinism is not required.

## How we work

- Fun first: build small playable steps, try them, then grow. Don't build deep systems before
  the moment-to-moment play is fun.
- Jim judges the game by playing it in Unreal. Show progress on screen.
- Jim prefers concise answers and plain language.
