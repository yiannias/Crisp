# Hotkey registration conflict

When applying settings, Crisp reported that Select a region (Print Screen), Scrolling capture (Ctrl+Shift+Print Screen), and All monitors (Ctrl+Alt+Shift+Print Screen) could not be registered. This is caused by another running application owning the keys, not by compilation. Avoid launching two Crisp instances; Windows Snipping Tool can also claim Print Screen.
