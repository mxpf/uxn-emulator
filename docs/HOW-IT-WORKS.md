# How the emulator works

The emulator sits between a Uxn program and the real computer. The program
only needs to understand Uxn and Varvara. Our host translates those small,
standard ideas into things the Mac can do.

```mermaid
flowchart TD
    A["You open a ROM<br/>A file containing the program's bytes"]
    B["The emulator host<br/>Loads those bytes and starts the window or terminal"]
    C["Uxn<br/>Reads one instruction at a time<br/>and works with memory and two small stacks"]
    D["Varvara devices<br/>The standard bridge between Uxn and the real computer"]
    E["Screen device<br/>Turns numbers into pixels"]
    F["Audio devices<br/>Turn numbers into sound"]
    G["Controller and mouse devices<br/>Carry your input back to the program"]
    H["File and console devices<br/>Read or write only where the host allows"]
    I["Window"]
    J["Speakers"]
    K["Keyboard, mouse,<br/>or game controller"]
    L["Local folder<br/>or terminal"]

    A --> B
    B --> C
    C -->|asks for screen, sound, or files| D
    D -->|returns input and results| C
    D --> E --> I
    D --> F --> J
    K --> G --> D
    D <--> H <--> L
```

This separation is why the same ROM can run on different computers. The Uxn
part stays the same. Each emulator only has to translate Varvara's devices into
the window, speakers, controls, and files available on its own machine.
