import { useEffect, useRef, useState } from "preact/hooks";
import Keyboard from "simple-keyboard";
import "simple-keyboard/build/css/index.css";

export type VirtualSpecialKey =
  | "backspace"
  | "tab"
  | "enter"
  | "left"
  | "right"
  | "home"
  | "end";

interface VirtualKeyboardProps {
  disabled?: boolean;
  onTextKey: (text: string) => void | Promise<void>;
  onSpecialKey: (key: VirtualSpecialKey) => void | Promise<void>;
}

function toggleLayout(name: string): string {
  return name === "default" ? "shift" : "default";
}

export function VirtualKeyboard({
  disabled,
  onTextKey,
  onSpecialKey,
}: VirtualKeyboardProps) {
  const hostRef = useRef<HTMLDivElement | null>(null);
  const keyboardRef = useRef<Keyboard | null>(null);
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

  useEffect(() => {
    if (!hostRef.current) return;

    keyboardRef.current = new Keyboard(hostRef.current, {
      layoutName: "default",
      layout: {
        default: [
          "1 2 3 4 5 6 7 8 9 0",
          "q w e r t y u i o p",
          "a s d f g h j k l",
          "{shift} z x c v b n m {bksp}",
          "{tab} {space} {enter}",
          "{home} {left} {right} {end}",
        ],
        shift: [
          "! @ # $ % ^ & * ( )",
          "Q W E R T Y U I O P",
          "A S D F G H J K L",
          "{shift} Z X C V B N M {bksp}",
          "{tab} {space} {enter}",
          "{home} {left} {right} {end}",
        ],
      },
      display: {
        "{shift}": "Shift",
        "{bksp}": "Backspace",
        "{tab}": "Tab",
        "{space}": "Space",
        "{enter}": "Enter",
        "{home}": "Home",
        "{left}": "Left",
        "{right}": "Right",
        "{end}": "End",
      },
      onKeyPress: (button: string) => {
        if (disabledRef.current) return;

        if (button === "{shift}") {
          const nextLayout = toggleLayout(layoutNameRef.current);
          setLayoutName(nextLayout);
          return;
        }

        if (button === "{bksp}") {
          void onSpecialKeyRef.current("backspace");
          return;
        }
        if (button === "{tab}") {
          void onSpecialKeyRef.current("tab");
          return;
        }
        if (button === "{enter}") {
          void onSpecialKeyRef.current("enter");
          return;
        }
        if (button === "{left}") {
          void onSpecialKeyRef.current("left");
          return;
        }
        if (button === "{right}") {
          void onSpecialKeyRef.current("right");
          return;
        }
        if (button === "{home}") {
          void onSpecialKeyRef.current("home");
          return;
        }
        if (button === "{end}") {
          void onSpecialKeyRef.current("end");
          return;
        }
        if (button === "{space}") {
          void onTextKeyRef.current(" ");
          return;
        }

        if (!button.startsWith("{")) {
          void onTextKeyRef.current(button);
        }
      },
    });

    return () => {
      keyboardRef.current?.destroy();
      keyboardRef.current = null;
    };
  }, []);

  useEffect(() => {
    keyboardRef.current?.setOptions({ layoutName });
    layoutNameRef.current = layoutName;
  }, [layoutName]);

  return (
    <div
      style={{
        marginTop: "0.75rem",
        padding: "0.75rem",
        background: "#0f172a",
        border: "1px solid #334155",
        borderRadius: "10px",
        opacity: disabled ? 0.6 : 1,
      }}
    >
      <div
        ref={hostRef}
        style={{
          width: "100%",
          maxWidth: "100%",
          touchAction: "manipulation",
        }}
      />
    </div>
  );
}
