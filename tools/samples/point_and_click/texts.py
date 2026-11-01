"""
Every text of The Moon Lantern in English and French, by table and key, written to ``localisation.yml`` by
``generate_scenes``. A text with ``%s`` or ``%d`` is formatted by the scripts.
"""

from __future__ import annotations

STRINGS: dict[str, dict[str, tuple[str, str]]] = {
    "menu": {
        "title.name": ("The Moon Lantern", "La Lanterne de Lune"),
        "title.tagline": ("A small fairy tale, made with Owl Engine", "Un petit conte de fées, fait avec Owl Engine"),
        "title.new": ("New game", "Nouvelle partie"),
        "title.continue": ("Continue", "Continuer"),
        "title.settings": ("Settings", "Réglages"),
        "title.quit": ("Quit", "Quitter"),
        "settings.title": ("Settings", "Réglages"),
        "settings.master": ("Master volume: %d%%", "Volume général : %d %%"),
        "settings.music": ("Music: %d%%", "Musique : %d %%"),
        "settings.effects": ("Effects: %d%%", "Effets : %d %%"),
        "settings.language": ("Language: English", "Langue : français"),
        "settings.subtitles_on": ("Subtitles: on", "Sous-titres : oui"),
        "settings.subtitles_off": ("Subtitles: off", "Sous-titres : non"),
        "settings.reset": ("Reset defaults", "Réglages par défaut"),
        "settings.back": ("Back", "Retour"),
        "ending.line1": (
            "The moonstone wakes, the fireflies give it their spark...",
            "La pierre de lune s'éveille, les lucioles lui donnent leur étincelle...",
        ),
        "ending.line2": (
            "...and the Moon Lantern shines over the whole valley again.",
            "...et la Lanterne de Lune brille de nouveau sur toute la vallée.",
        ),
        "ending.line3": (
            "Tonight the fairies will find their way home. And Barnaby will finally find his slippers.",
            "Cette nuit, les fées retrouveront leur chemin. Et Barnaby retrouvera enfin ses pantoufles.",
        ),
        "ending.end": ("The End", "Fin"),
        "ending.title": ("Back to the title", "Retour au titre"),
    },
    "hud": {
        "hud.bag": ("Bag", "Sac"),
        "hud.next": ("Next", "Suite"),
        "pause.title": ("Pause", "Pause"),
        "pause.resume": ("Resume", "Reprendre"),
        "pause.save": ("Save", "Sauvegarder"),
        "pause.quit": ("Title screen", "Écran titre"),
        "verb.look": ("Look at %s", "Regarder %s"),
        "verb.pick": ("Pick up %s", "Prendre %s"),
        "verb.use": ("Use %s", "Utiliser %s"),
        "verb.talk": ("Talk to %s", "Parler à %s"),
        "verb.exit": ("Go to %s", "Aller vers %s"),
        "verb.use_on": ("Use %s on %s", "Utiliser %s sur %s"),
        "subtitle.door": ("[the door creaks in three keys]", "[la porte grince sur trois notes]"),
        "subtitle.fireflies": ("[the fireflies hum in the jar]", "[les lucioles bourdonnent dans le bocal]"),
        "subtitle.pie": ("[a satisfied squelch]", "[un floc satisfait]"),
        "subtitle.munch": ("[munch, munch, munch]", "[miam, miam, miam]"),
    },
    "items": {
        "item.jar": ("the empty jar", "le bocal vide"),
        "item.firefly_jar": ("the jar of fireflies", "le bocal de lucioles"),
        "item.berries": ("the moonberries", "les baies de lune"),
        "item.crust": ("the pie crust", "la pâte à tarte"),
        "item.pie": ("the moonberry pie", "la tarte aux baies de lune"),
        "item.moonstone": ("the moonstone", "la pierre de lune"),
    },
    "rooms": {
        "name.cottage.jar": ("the jar", "le bocal"),
        "name.cottage.crust": ("the pie crust", "la pâte à tarte"),
        "name.cottage.barnaby": ("Barnaby", "Barnaby"),
        "name.cottage.lantern": ("the Moon Lantern", "la Lanterne de Lune"),
        "name.cottage.door": ("the glade", "la clairière"),
        "name.cottage.window": ("the window", "la fenêtre"),
        "name.cottage.fireplace": ("the fireplace", "la cheminée"),
        "name.cottage.shelf": ("the shelf", "l'étagère"),
        "name.cottage.table": ("the table", "la table"),
        "look.cottage.jar": (
            "An empty jar. Full of nothing, which is the best way to start.",
            "Un bocal vide. Plein de rien, ce qui est un bon début.",
        ),
        "look.cottage.crust": (
            "A pie crust, waiting for a filling. It looks a little lonely.",
            "Une pâte à tarte qui attend sa garniture. Elle a l'air un peu seule.",
        ),
        "look.cottage.barnaby": (
            "Barnaby, my master. A great wizard, if you do not count the spells.",
            "Barnaby, mon maître. Un grand magicien, sans compter les sortilèges.",
        ),
        "look.cottage.lantern": (
            "The Moon Lantern. It should shine for the fairies tonight. It is as dark as a boot.",
            "La Lanterne de Lune. Elle devrait briller pour les fées ce soir. Elle est noire comme une botte.",
        ),
        "look.cottage.door": (
            "The door to the glade. It creaks in three different keys.",
            "La porte de la clairière. Elle grince sur trois notes différentes.",
        ),
        "look.cottage.window": (
            "A clear night. The moon is out, but the lantern is not.",
            "Une nuit claire. La lune est là, mais pas la lanterne.",
        ),
        "look.cottage.fireplace": (
            "A cosy fire. Barnaby says it is magic. I saw him use matches.",
            "Un bon feu. Barnaby dit qu'il est magique. Je l'ai vu utiliser des allumettes.",
        ),
        "look.cottage.shelf": (
            "Books of spells, a potion of hiccups and dust. Mostly dust.",
            "Des grimoires, une potion de hoquet et de la poussière. Surtout de la poussière.",
        ),
        "look.cottage.table": (
            "Barnaby's table. It has seen more spilled soup than spells.",
            "La table de Barnaby. Elle a vu plus de soupe renversée que de sortilèges.",
        ),
        "name.glade.fireflies": ("the fireflies", "les lucioles"),
        "name.glade.bush": ("the berry bush", "le buisson de baies"),
        "name.glade.mushrooms": ("the mushroom ring", "le cercle de champignons"),
        "name.glade.pond": ("the pond", "la mare"),
        "name.glade.to_cottage": ("the cottage", "la chaumière"),
        "name.glade.to_bridge": ("the bridge", "le pont"),
        "look.glade.fireflies": (
            "Fireflies dancing over the pond. Tiny sparks with wings.",
            "Des lucioles qui dansent sur la mare. De minuscules étincelles avec des ailes.",
        ),
        "look.glade.bush": (
            "A moonberry bush. Sweet, purple, and very fond of staining shirts.",
            "Un buisson de baies de lune. Sucrées, violettes, et très douées pour tacher les chemises.",
        ),
        "look.glade.mushrooms": (
            "A fairy ring. Stepping inside makes you dance for a hundred years. I will pass.",
            "Un cercle de fées. Y entrer fait danser cent ans. Je vais m'abstenir.",
        ),
        "look.glade.pond": (
            "The pond. A frog looks at me as if I owed it money.",
            "La mare. Une grenouille me regarde comme si je lui devais de l'argent.",
        ),
        "look.glade.to_cottage": ("The way back to the cottage.", "Le chemin de la chaumière."),
        "look.glade.to_bridge": ("The path to the troll bridge.", "Le sentier du pont du troll."),
        "name.bridge.moonstone": ("the moonstone", "la pierre de lune"),
        "name.bridge.troll": ("the troll", "le troll"),
        "name.bridge.sign": ("the sign", "le panneau"),
        "name.bridge.garden": ("the garden", "le potager"),
        "name.bridge.stream": ("the stream", "le ruisseau"),
        "name.bridge.to_glade": ("the glade", "la clairière"),
        "look.bridge.moonstone": (
            "The moonstone! A little warm. I prefer not to know why.",
            "La pierre de lune ! Un peu tiède. Je préfère ne pas savoir pourquoi.",
        ),
        "look.bridge.troll": (
            "The bridge troll. Big, green, and sitting on something shiny.",
            "Le troll du pont. Grand, vert, et assis sur quelque chose de brillant.",
        ),
        "look.bridge.sign": (
            "A sign with a pie painted on it. With love, and very little talent.",
            "Un panneau avec une tarte peinte dessus. Avec amour, et très peu de talent.",
        ),
        "look.bridge.garden": (
            "The troll's garden: turnips in very straight rows. He must be proud.",
            "Le potager du troll : des navets en rangs très droits. Il doit être fier.",
        ),
        "look.bridge.stream": (
            "The stream babbles. It has nothing much to say, but it says it well.",
            "Le ruisseau gazouille. Il n'a pas grand-chose à dire, mais il le dit bien.",
        ),
        "look.bridge.to_glade": ("The path back to the glade.", "Le sentier de la clairière."),
    },
    "remarks": {
        "say.no": ("I would rather not.", "Je préfère éviter."),
        "say.use_no": ("Using %s here would not help.", "Utiliser %s ici ne servirait à rien."),
        "say.saved": ("The game is saved.", "Partie sauvegardée."),
        "say.combine.no": ("These two do not go together.", "Ces deux-là ne vont pas ensemble."),
        "say.combine.pie": (
            "Moonberries in the crust: a moonberry pie! My best cooking ever. My only one, too.",
            "Des baies de lune dans la pâte : une tarte ! Ma meilleure recette. La seule, aussi.",
        ),
        "say.cottage.intro": (
            "The Moon Lantern has gone dark... I should ask Barnaby what happened.",
            "La Lanterne de Lune s'est éteinte... Je devrais demander à Barnaby.",
        ),
        "say.cottage.jar": (
            "An empty jar. It could hold something small and bright.",
            "Un bocal vide. Il pourrait contenir quelque chose de petit et brillant.",
        ),
        "say.cottage.crust": (
            "A pie crust. A pie without filling is just a sad plate.",
            "Une pâte à tarte. Une tarte sans garniture n'est qu'une assiette triste.",
        ),
        "say.cottage.barnaby_item": (
            'Barnaby: "Keep it, my boy. I am on a diet of tea and mystery."',
            "Barnaby : « Garde-le, mon garçon. Je suis au régime thé et mystère. »",
        ),
        "say.cottage.lantern_needs_stone": (
            "The lantern is empty: its moonstone is gone.",
            "La lanterne est vide : sa pierre de lune a disparu.",
        ),
        "say.cottage.lantern_needs_spark": (
            "The moonstone is in place, but it sleeps. It needs a spark.",
            "La pierre de lune est en place, mais elle dort. Il lui faut une étincelle.",
        ),
        "say.cottage.lantern_item": ("That will not light the lantern.", "Ça n'allumera pas la lanterne."),
        "say.cottage.lantern_stone": (
            "The moonstone clicks into place. Now, a spark...",
            "La pierre de lune se met en place. Maintenant, une étincelle...",
        ),
        "say.cottage.lantern_stone_last": (
            "The moonstone clicks into place, right above the fireflies...",
            "La pierre de lune se met en place, juste au-dessus des lucioles...",
        ),
        "say.cottage.lantern_spark": (
            "The fireflies settle in the lantern. They wait for the moonstone.",
            "Les lucioles s'installent dans la lanterne. Elles attendent la pierre.",
        ),
        "say.cottage.lantern_spark_last": (
            "The fireflies fly to the moonstone...",
            "Les lucioles volent vers la pierre de lune...",
        ),
        "say.glade.bush_item": ("The bush does not need that.", "Le buisson n'en a pas besoin."),
        "say.glade.bush_empty": (
            "Only leaves now. And a very disappointed beetle.",
            "Plus que des feuilles. Et un scarabée très déçu.",
        ),
        "say.glade.berries": (
            "A handful of moonberries. Sweet enough for a troll?",
            "Une poignée de baies de lune. Assez sucrées pour un troll ?",
        ),
        "say.glade.fireflies_caught": (
            "A jar full of sparks! They do not seem to mind. They like the view.",
            "Un bocal plein d'étincelles ! Elles ne s'en plaignent pas. Elles aiment la vue.",
        ),
        "say.glade.fireflies_enough": (
            "I have enough fireflies. The others have families.",
            "J'ai assez de lucioles. Les autres ont de la famille.",
        ),
        "say.glade.fireflies_item": ("The fireflies are not interested.", "Les lucioles ne sont pas intéressées."),
        "say.glade.fireflies_hands": (
            "Too quick for bare hands. I need something to keep them in.",
            "Trop rapides pour mes mains. Il me faut de quoi les garder.",
        ),
        "say.glade.mushrooms_touch": (
            "No dancing for a hundred years, thank you.",
            "Pas de danse pendant cent ans, merci.",
        ),
        "say.bridge.troll_full": (
            'The troll: "Toll paid! Toll paid! Go away, I am digesting."',
            "Le troll : « Péage payé ! Péage payé ! Va-t'en, je digère. »",
        ),
        "say.bridge.troll_half": (
            'The troll: "Half a pie is not a pie! Toll is a WHOLE sweet thing!"',
            "Le troll : « Une moitié de tarte n'est pas une tarte ! Le péage, c'est une chose sucrée ENTIÈRE ! »",
        ),
        "say.bridge.troll_item": (
            'The troll sniffs it: "Not sweet! Not toll!"',
            "Le troll renifle : « Pas sucré ! Pas péage ! »",
        ),
        "say.bridge.troll_paid": (
            'The troll: "Toll paid! Take the cold stone, it chilled my bottom anyway."',
            "Le troll : « Péage payé ! Prends la pierre froide, elle me gelait le derrière. »",
        ),
        "say.bridge.moonstone": (
            "The moonstone! Now back to the lantern.",
            "La pierre de lune ! Retour à la lanterne.",
        ),
    },
    "dialogue": {
        "dialogue.barnaby.name": ("Barnaby", "Barnaby"),
        "dialogue.barnaby.hello": (
            "Ah, Wren! The Moon Lantern has gone dark. Without it the fairies will never find the valley tonight.",
            "Ah, Wren ! La Lanterne de Lune s'est éteinte. Sans elle, les fées ne trouveront "
            "jamais la vallée cette nuit.",
        ),
        "dialogue.barnaby.ask": ("What troubles you, my boy?", "Qu'est-ce qui te tracasse, mon garçon ?"),
        "dialogue.barnaby.answer_stone": ("Where is its moonstone?", "Où est sa pierre de lune ?"),
        "dialogue.barnaby.answer_help": ("What else does it need?", "De quoi a-t-elle besoin d'autre ?"),
        "dialogue.barnaby.answer_bye": ("I will fix it!", "Je vais la réparer !"),
        "dialogue.barnaby.stone": (
            "The bridge troll took it. He sits on everything shiny. And he has a terrible sweet tooth.",
            "Le troll du pont l'a prise. Il s'assoit sur tout ce qui brille. Et il est terriblement gourmand.",
        ),
        "dialogue.barnaby.spark": (
            "A moonstone sleeps without a spark. Fireflies make lovely sparks, if you can keep them in something.",
            "Une pierre de lune dort sans étincelle. Les lucioles en font de très jolies, si "
            "on peut les garder dans quelque chose.",
        ),
        "dialogue.barnaby.ready": (
            "You have all it needs! Set it all in the lantern, quick.",
            "Tu as tout ce qu'il faut ! Mets tout ça dans la lanterne, vite.",
        ),
        "dialogue.barnaby.bye": (
            "Mind the troll's manners. And yours.",
            "Attention aux manières du troll. Et aux tiennes.",
        ),
        "dialogue.wren.name": ("Wren", "Wren"),
        "dialogue.troll.name": ("Troll", "Troll"),
        "dialogue.troll.hello": (
            "TOLL! TOLL! Nobody crosses my bridge without paying the toll!",
            "PÉAGE ! PÉAGE ! Personne ne passe sur mon pont sans payer le péage !",
        ),
        "dialogue.troll.ask": ("Well? Toll?", "Alors ? Péage ?"),
        "dialogue.troll.answer_toll": ("What is the toll?", "C'est quoi, le péage ?"),
        "dialogue.troll.answer_stone": (
            "You are sitting on our moonstone.",
            "Vous êtes assis sur notre pierre de lune.",
        ),
        "dialogue.troll.answer_pie": ("Would a pie do?", "Une tarte, ça irait ?"),
        "dialogue.troll.answer_bye": ("Never mind.", "Laissez tomber."),
        "dialogue.troll.toll": (
            "Something sweet. Very sweet. Sweeter than my mother's turnips.",
            "Quelque chose de sucré. Très sucré. Plus sucré que les navets de ma mère.",
        ),
        "dialogue.troll.stone": (
            "This? A very comfortable stone. Nobody gets my comfortable stone without the toll.",
            "Ça ? Une pierre très confortable. Personne n'aura ma pierre confortable sans le péage.",
        ),
        "dialogue.troll.pie": ("A PIE? Give it! Give it now!", "Une TARTE ? Donne ! Donne tout de suite !"),
        "dialogue.troll.bye": ("TOLL!", "PÉAGE !"),
    },
}
"""Texts by table, then by key: (English, French)."""
