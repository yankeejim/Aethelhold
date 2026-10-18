# Aethelhold: concept

Agreed with Jim, 2026-10-18. This is the foundation; later documents go into detail.

## The pitch

A high-fantasy city-builder. A handful of founders arrive at a wild site and grow, over
generations, into a city of thousands. The city has a ruler, and around the ruler stand
nobles, guild masters, priests and generals with minds of their own, who scheme, ally and
fight over power and over who rules next. Rival kingdoms press on the borders, and now and
then a monster comes. The city itself is a constant pressure: fire, plague, hunger and
unrest. The player is the unseen hand that plans and manages it. An endless sandbox.

Inspirations: Songs of Syx (scale), Dwarf Fortress and RimWorld (deep simulation, stories
that emerge on their own), with graphics those games never had, in Unreal Engine 5.

## What makes it different

- **People with minds.** The city's notables think with a local AI model: they remember,
  hold grudges, form factions, plot succession and explain themselves in their own words.
- **A ruler the player doesn't control.** The player builds the city; the people in power
  decide what to do with it.
- **Politics through the city.** What the player builds shifts who holds power.
- **It looks good.** The depth of a colony sim, in a world worth looking at.

## The player's role

The player is an unseen manager, as hands-on as in Songs of Syx, Dwarf Fortress and
RimWorld. The practical city belongs to the player:

- **Building:** where every building, wall, road and bridge goes, and from what material
  (wood, stone, brick, marble).
- **Farming:** fields and their crops, pastures, orchards, fishing.
- **Production:** workshops and what they make, how much, and stockpiles.
- **Work:** job priorities, and who works where when the player wants to manage it.
- **Housing and services:** homes, wells, taverns, bathhouses, hospitals, graveyards.
- **Defense:** walls, towers, gates, where the guard stands, and commanding the defenders
  when the city is attacked.

## The people

Two layers, so a city of thousands can still have real politics:

- **The crowd:** thousands of citizens with needs, jobs, homes, families and moods,
  simulated by fast rules. They do the work, choose among themselves who does what, eat,
  sleep, fall in love, quarrel, and form opinions about the city and its rulers.
- **The notables:** a few dozen named characters with AI minds: the ruler, the heirs,
  nobles, guild masters, the high priest, the general, a rabble-rouser. They hold the
  power and drive the politics.

The layers meet in a crisis: hungry crowds back a rabble-rouser, the army follows its
general, a plague kills the heir.

## Politics and succession

The ruler and council control the political city: taxes, laws, war and peace, treaties,
and offices. The player can't order them. Their interventions are a few big moments a year,
not constant meddling:

- **Demands:** "build me a palace", "raise a statue of my father", "double the guard".
  Meeting them earns favour; refusing costs it.
- **Decrees:** ban a building type, forbid a trade, set curfews.
- **Taxes and seizure:** a share of what the city produces goes to the treasury, the palace
  or the army.
- **Conscription:** workers pulled into the army.
- **War and treaties,** made on their own.
- **Appointments:** cronies placed over the guilds, the temple or the guard.

They always give their reasons, and sometimes they are right and the player is wrong.

The player's levers are all indirect, all through the city:

1. **What gets built shifts power:** a great temple strengthens the priests, barracks and
   walls the general, markets and workshops the guilds, a palace the ruler.
2. **The ruler's purse:** the treasury lives on what the city produces.
3. **The people's mood:** fed, housed, safe citizens back whoever rules; hungry, angry
   ones listen to a rabble-rouser.
4. **Favourites:** the best house, a workshop and guards raise someone up.
5. **The council:** big decrees need votes, and the factions the player has strengthened
   hold them.
6. **Succession:** rulers age, sicken, are assassinated or overthrown, and the factions
   decide who takes the throne.

**Rule of thumb:** a ruler can make things harder, costlier or slower, but never take the
city out of the player's hands. If things go badly enough, the city can fall to
rebellion, conquest or ruin, and that is a story too.

## Bloodlines

The founders establish family dynasties. Every citizen's ancestry is tracked from day one.
Descent from a founder carries latent legitimacy: in year 100, a destitute dockworker whose
great-great-grandfather was Founder #1 has a stronger claim than an upstart merchant king.
When unrest peaks, the crowd can rally behind the "True Blood" sleeping in the slums.

## Haunted infrastructure

When a ruler dies violently in the palace, or a notable is crushed in a rushed cathedral
collapse, a summary of their memory attaches to that building. Whoever later sleeps or works
there is touched by it: dreams, paranoia, or personality drift driven by the dead notable's
past. The player then decides what to do with the place: use it, seal it, or tear it down.

## The judgment hall

Grievances between citizens, guilds and nobles don't stay in council votes. They come to
the judgment hall. The player sits there as the ruler's unseen counsel, "the Steward": the
ruler usually follows the Steward's verdict, but a ruler who distrusts the Steward, or has a
stake in the case, may overrule it. Seizing an asset for the crown needs the ruler's will.

```
Dispute emerges (crowd or notables)
  -> Public hearing (the AI notables brief their cases)
    -> The player's ruling
      -> Enforced by the town guard or bailiff
        -> A precedent is set (shifts behaviour across the city)
```

- **Triggers:** a guild master accuses the high priest of stealing tithes; miners accuse the
  stonemasons' guild of unsafe tunnelling beneath their homes.
- **Presentation:** the claimants testify in their own words, grounded in real events in the
  simulation ("His forge smoke ruined our sacred orchard on day 42").
- **Rulings:**
  - **In favour of one side:** awards damages, shifts faction favour, adjusts tariffs.
  - **Compromise:** a forced settlement (the masons supply the temple with cut stone for free).
  - **Dismiss both (autocratic):** throw both out, or seize the disputed asset for the crown.
- **Precedent:** rulings become law. Siding with guilds over the temple permanently shifts
  tax compliance and devotion across the city.

## Monsters and expeditions

When an elder beast or mythical threat emerges in the wild, dealing with it links the
city's economy to the frontier:

```
Sighting or lair discovered
  -> The council clashes over the response (hunt or appease)
    -> The player provisions and outfits the warband
      -> The expedition into the wild (led by the general)
        -> Aftermath: trophies, arcane relics, or retaliation
```

- **The dilemma:** the general wants a glorious crusade; the high priest wants the beast
  pacified with cattle sacrifices; the merchants want to trade for its venom or scales.
- **Logistics:** the player designs the pack-train. How many masterwork polearms? Send the
  elite city guard, and leave the streets open to the thieves' syndicate?
- **Trophies:** the carcass is carted through the gates, and where the player puts it shapes
  the city. Mounted in the town square, it raises bravery and reduces panic in future raids.
  Delivered to the arcane scriptorium or the alchemists, it unlocks high-fantasy
  infrastructure (dragon-blood heated aqueducts that keep crops growing through winter).

## War

- **When the city is attacked** (raiders, a siege, a monster), the player commands the
  defenders.
- **When the ruler sends an army away,** the general leads the campaign. The player arms,
  feeds and supplies it, and sees it come home in glory, in pieces, or not at all.
- A general who wins abroad comes home a hero, and may become a rival for the throne.

## The world

- **High fantasy.**
- **Races:** the player chooses the city's race at the start. Other races live on the map
  as neighbours, traders, rivals and threats.
- **Threats:** rival kingdoms (diplomacy, raids, war) and an occasional monster.
- **Endless sandbox,** growing from a handful of founders, as in Dwarf Fortress and Songs
  of Syx.

## Look

Like Manor Lords: grounded, natural and detailed, with real terrain, forests, weather and
seasons, warm natural light, and medieval-style buildings that rise in visible stages.

## How it will be built

- **Unreal Engine 5, C++,** simulation and graphics in one place: thousands of citizens
  need the speed, and Unreal's Mass system handles large crowds.
- **The notables think with a local AI model** on the player's PC (Ollama, qwen2.5 7b or
  14b, already installed on Jim's PC). The crowd runs on fast rules.
- **Fun first.** Small playable steps, each tried before the next is built.

## First playable milestone: the founding

Seven founders arrive at a wild site and must survive their first winter:

- a small map with forest, stone, water and wildlife;
- the player designates what to gather, what to build and where;
- founders with needs (food, warmth, rest), skills and personalities, doing the work
  themselves;
- the first leader among them;
- the first cases in the judgment hall (who ate the stored berries, who gets the warm spot by
  the fire) and the first precedents;
- the founders' bloodlines tracked from day one;
- the first winter as the threat.

When that is fun to play, the city grows: migrants, the first notables, the first
political quarrel.

## Open questions

- Which races exist, and what each one is like (the first test uses humans only).
- How magic works, and who has it.
- How rival kingdoms are shown: on a world map, or only through what reaches the city.
- How the player is told what the notables think (without a wall of text).
