import { useEffect, useRef, useState } from "preact/hooks";
import Keyboard from "simple-keyboard";
import "simple-keyboard/build/css/index.css";
import "./VirtualKeyboard.css";

export type VirtualSpecialKey =
  | "backspace"
  | "tab"
  | "enter"
  | "escape"
  | "print_screen"
  | "scroll_lock"
  | "pause"
  | "insert"
  | "delete"
  | "page_up"
  | "page_down"
  | "up"
  | "down"
  | "left"
  | "right"
  | "home"
  | "end"
  | "ctrl"
  | "alt"
  | "cmd"
  | "f1"
  | "f2"
  | "f3"
  | "f4"
  | "f5"
  | "f6"
  | "f7"
  | "f8"
  | "f9"
  | "f10"
  | "f11"
  | "f12";

export type KeyboardLayoutVariant = "simple" | "full";

interface VirtualKeyboardProps {
  disabled?: boolean;
  layoutVariant?: KeyboardLayoutVariant;
  onTextKey: (text: string) => void | Promise<void>;
  onSpecialKey: (key: VirtualSpecialKey) => void | Promise<void>;
}

const SIMPLE_LAYOUT = {
  default: [
    "1 2 3 4 5 6 7 8 9 0",
    "q w e r t y u i o p",
    "a s d f g h j k l",
    "{shift} z x c v b n m {backspace}",
    "{tab} {space} {enter}",
    "{home} {left} {right} {end}",
  ],
  shift: [
    "! @ # $ % ^ & * ( )",
    "Q W E R T Y U I O P",
    "A S D F G H J K L",
    "{shift} Z X C V B N M {backspace}",
    "{tab} {space} {enter}",
    "{home} {left} {right} {end}",
  ],
};

const FULL_MAIN_LAYOUT = {
  default: [
    "{escape} {f1} {f2} {f3} {f4} {f5} {f6} {f7} {f8} {f9} {f10} {f11} {f12}",
    "` 1 2 3 4 5 6 7 8 9 0 - = {backspace}",
    "{tab} q w e r t y u i o p [ ] \\",
    "{capslock} a s d f g h j k l ; ' {enter}",
    "{shiftleft} z x c v b n m , . / {shiftright}",
    "{controlleft} {altleft} {metaleft} {space} {metaright} {altright}",
  ],
  shift: [
    "{escape} {f1} {f2} {f3} {f4} {f5} {f6} {f7} {f8} {f9} {f10} {f11} {f12}",
    "~ ! @ # $ % ^ & * ( ) _ + {backspace}",
    "{tab} Q W E R T Y U I O P { } |",
    "{capslock} A S D F G H J K L : \" {enter}",
    "{shiftleft} Z X C V B N M < > ? {shiftright}",
    "{controlleft} {altleft} {metaleft} {space} {metaright} {altright}",
  ],
};

const FULL_CONTROL_LAYOUT = {
  default: [
    "{prtscr} {scrolllock} {pause}",
    "{insert} {home} {pageup}",
    "{delete} {end} {pagedown}",
  ],
};

const FULL_ARROWS_LAYOUT = {
  default: ["{arrowup}", "{arrowleft} {arrowdown} {arrowright}"],
};

const DISPLAY = {
  "{escape}": "esc ⎋",
  "{tab}": "tab ⇥",
  "{backspace}": "backspace ⌫",
  "{enter}": "enter ↵",
  "{capslock}": "caps lock ⇪",
  "{shift}": "shift ⇧",
  "{shiftleft}": "shift ⇧",
  "{shiftright}": "shift ⇧",
  "{controlleft}": "ctrl ⌃",
  "{controlright}": "ctrl ⌃",
  "{altleft}": "alt ⌥",
  "{altright}": "alt ⌥",
  "{metaleft}": "cmd ⌘",
  "{metaright}": "cmd ⌘",
  "{prtscr}": "print",
  "{scrolllock}": "scroll",
  "{pause}": "pause",
  "{insert}": "ins",
  "{delete}": "del",
  "{pageup}": "up",
  "{pagedown}": "down",
  "{arrowup}": "↑",
  "{arrowleft}": "←",
  "{arrowdown}": "↓",
  "{arrowright}": "→",
};

function toggleLayout(name: string): string {
  return name === "default" ? "shift" : "default";
}

function mapTokenToSpecialKey(token: string): VirtualSpecialKey | null {
  switch (token) {
    case "{backspace}":
      return "backspace";
    case "{tab}":
      return "tab";
    case "{enter}":
      return "enter";
    case "{escape}":
      return "escape";
    case "{prtscr}":
      return "print_screen";
    case "{scrolllock}":
      return "scroll_lock";
    case "{pause}":
      return "pause";
    case "{insert}":
      return "insert";
    case "{delete}":
      return "delete";
    case "{pageup}":
      return "page_up";
    case "{pagedown}":
      return "page_down";
    case "{arrowup}":
      return "up";
    case "{arrowdown}":
      return "down";
    case "{arrowleft}":
      return "left";
    case "{arrowright}":
      return "right";
    case "{home}":
      return "home";
    case "{end}":
      return "end";
    case "{controlleft}":
    case "{controlright}":
      return "ctrl";
    case "{altleft}":
    case "{altright}":
      return "alt";
    case "{metaleft}":
    case "{metaright}":
      return "cmd";
    default:
      return null;
  }
}

function createKeyboardInstance(
  selector: string,
  options: Record<string, unknown>
): Keyboard | null {
  if (!document.querySelector(selector)) return null;
  return new Keyboard(selector, options);
}

export function VirtualKeyboard({
  disabled,
  layoutVariant = "simple",
  onTextKey,
  onSpecialKey,
}: VirtualKeyboardProps) {
  const mainKeyboardRef = useRef<Keyboard | null>(null);
  const controlKeyboardRef = useRef<Keyboard | null>(null);
  const arrowsKeyboardRef = useRef<Keyboard | null>(null);

  const disabledRef = useRef(Boolean(disabled));
  const onTextKeyRef = useRef(onTextKey);
  const onSpecialKeyRef = useRef(onSpecialKey);
  const [layoutName, setLayoutName] = useState("default");
  const layoutNameRef = useRef("default");

  useEffect(() => {
    disabledRef.current = Boolean(disabled);
  }, [disabled]);

  useEffect(() => {
    onTextKeyRef.current = onTextKey;
  }, [onTextKey]);

  useEffect(() => {
    onSpecialKeyRef.current = onSpecialKey;
  }, [onSpecialKey]);

  const handleKeyPress = (button: string) => {
    if (disabledRef.current) return;

    if (
      button === "{shift}" ||
      button === "{shiftleft}" ||
      button === "{shiftright}" ||
      button === "{capslock}"
    ) {
      const nextLayout = toggleLayout(layoutNameRef.current);
      setLayoutName(nextLayout);
      return;
    }

    const mappedSpecial = mapTokenToSpecialKey(button);
    if (mappedSpecial) {
      void onSpecialKeyRef.current(mappedSpecial);
      return;
    }

    const functionMatch = button.match(/^\{f(1[0-2]|[1-9])\}$/);
    if (functionMatch) {
      void onSpecialKeyRef.current(`f${functionMatch[1]}` as VirtualSpecialKey);
      return;
    }

    if (button === "{space}") {
      void onTextKeyRef.current(" ");
      return;
    }

    if (!button.startsWith("{")) {
      void onTextKeyRef.current(button);
    }
  };

  useEffect(() => {
    let retryFrame: number | null = null;

    const destroyInstances = () => {
      mainKeyboardRef.current?.destroy();
      controlKeyboardRef.current?.destroy();
      arrowsKeyboardRef.current?.destroy();
      mainKeyboardRef.current = null;
      controlKeyboardRef.current = null;
      arrowsKeyboardRef.current = null;
    };

    const initialize = (): boolean => {
      destroyInstances();

      if (layoutVariant === "simple") {
        const main = createKeyboardInstance(".vk-simple-host", {
          layoutName: layoutNameRef.current,
          layout: SIMPLE_LAYOUT,
          display: DISPLAY,
          mergeDisplay: true,
          theme: "simple-keyboard hg-theme-default vk-theme vk-simple",
          onKeyPress: handleKeyPress,
        });
        mainKeyboardRef.current = main;
        return main !== null;
      }

      const main = createKeyboardInstance(".vk-full-main-host", {
        layoutName: layoutNameRef.current,
        layout: FULL_MAIN_LAYOUT,
        display: DISPLAY,
        mergeDisplay: true,
        theme: "simple-keyboard hg-theme-default vk-theme vk-full-main",
        onKeyPress: handleKeyPress,
      });
      const control = createKeyboardInstance(".vk-full-control-host", {
        layoutName: "default",
        layout: FULL_CONTROL_LAYOUT,
        display: DISPLAY,
        mergeDisplay: true,
        theme: "simple-keyboard hg-theme-default vk-theme vk-full-control",
        onKeyPress: handleKeyPress,
      });
      const arrows = createKeyboardInstance(".vk-full-arrows-host", {
        layoutName: "default",
        layout: FULL_ARROWS_LAYOUT,
        display: DISPLAY,
        mergeDisplay: true,
        theme: "simple-keyboard hg-theme-default vk-theme vk-full-arrows",
        onKeyPress: handleKeyPress,
      });

      mainKeyboardRef.current = main;
      controlKeyboardRef.current = control;
      arrowsKeyboardRef.current = arrows;
      return Boolean(main && control && arrows);
    };

    if (!initialize()) {
      retryFrame = window.requestAnimationFrame(() => {
        void initialize();
      });
    }

    return () => {
      if (retryFrame !== null) {
        window.cancelAnimationFrame(retryFrame);
      }
      destroyInstances();
    };
  }, [layoutVariant]);

  useEffect(() => {
    mainKeyboardRef.current?.setOptions({ layoutName });
    layoutNameRef.current = layoutName;
  }, [layoutName]);

  if (layoutVariant === "full") {
    return (
      <div
        className="vk-shell vk-full-shell"
        aria-disabled={disabled ? "true" : "false"}
      >
        <div className="vk-full-scroll">
          <div className="vk-full-canvas">
            <div className="vk-full-main-host" />
            <div className="vk-full-right">
              <div className="vk-full-control-host" />
              <div className="vk-full-arrows-host" />
            </div>
          </div>
        </div>
      </div>
    );
  }

  return (
    <div className="vk-shell" aria-disabled={disabled ? "true" : "false"}>
      <div className="vk-simple-host" />
    </div>
  );
}
