# YosysYodelers
Some Yosys plugins that are as pleasant as a hike through the alps! 🏔️🎶

Welcome to the YosysYodelers repository, where we embark on a delightful journey through the world of Yosys plugins. Just like a refreshing hike through the picturesque Alps, these plugins aim to make your digital circuit design experience a joyful and rewarding one.

## 🌄 Path Existence Plugin
The Path Existence plugin is your trusty companion in finding paths between specified signals in a digital circuit design. It's like having a knowledgeable guide leading you through the scenic routes of your circuit. With this plugin, you can:

- Specify the starting and ending signals for the path search
- Traverse the circuit using a depth-first search algorithm
- Discover the sequence of signals and cells along the path
- Generate a schematic representation of the found path

So, put on your hiking boots and let the Path Existence plugin be your guide to exploring the beautiful landscapes of your digital circuits!

## 🏔️ Path Isomorphism Plugin
The Path Isomorphism plugin is your companion in comparing the structural similarity of two paths in a digital circuit design. It's like having a keen eye for spotting the similarities between two breathtaking mountain trails. With this plugin, you can:

- Specify two starting signals and two ending signals for the isomorphism check
- Build a directed graph representation of the circuit
- Perform a topological sorting of the graph for proper traversal order
- Traverse the paths using a depth-first search algorithm
- Determine if the paths are isomorphic and report the findings

Let the Path Isomorphism plugin be your guide in discovering the hidden similarities and symmetries within your digital circuits!

## 🎶 Getting Started
To embark on this delightful journey with YosysYodelers, follow these steps:

1. Clone the repository:
   ```
   git clone https://github.com/your-username/YosysYodelers.git
   ```

2. Navigate to the desired plugin directory:
   ```
   cd path_existence
   ```
   or
   ```
   cd path_isomorphism
   ```

3. Compile the plugin:
   ```
   jupyter lab&
   ```

4. Load the plugin in Yosys:
   ```
   see the jupyter lab notebook
   ```

5. Enjoy the pleasant hike through your digital circuits with the YosysYodelers plugins!

## 📂 Repository Structure
The repository is structured as follows:

```
├── path_existence
│   ├── FindPath.ipynb
│   ├── compile
│   ├── main.cc
│   ├── out.dot
│   ├── out_small.dot
│   ├── path.d
│   ├── path.so
│   ├── top.il
│   └── top.sv
└─ path_isomorphism
    ├── PathIso.ipynb
    ├── compile
    ├── iso.d
    ├── iso.so
    ├── isomorphic.dot
    ├── main_opt.cc
    ├── out.dot
    └── top.sv
```

Each plugin has its own directory containing the necessary source files, compilation scripts, and example circuits.

So, grab your virtual hiking gear and let's explore the wonders of digital circuits with YosysYodelers! 🌿🏔️✨
