\name Gay Pride
\def black  #000000
\def white  #ffffff
\def yellow #ffff33
\def pink   #ff00cc
\def green  #00ff00
\def dkgreen#00cc00

  header: bg => @gaypride-(header).png, fg => black, line => yellow -1, accent => green
	  shadow => dkgreen, shine => green,
	  gradient.top => pink,
	  gradient.middle => pink,
	  gradient.bottom => dkgreen,
	  gradient.bar => green
   music: bar => dkgreen
 battery: border => pink, bg => yellow, fill.normal => pink +1, fill.low => dkgreen +1, fill.charge => pink +1,
	  bg.low => yellow, bg.charging => green, chargingbolt => pink
    lock: border => yellow, fill => yellow
 loadavg: bg => green, fg => dkgreen, spike => pink

  window: bg => pink, fg => yellow, border => green -3
  dialog: bg => pink, fg => yellow, line => yellow,
          title.fg => yellow,	
          button.bg => pink, button.fg => yellow, button.border => green,
	  button.sel.bg => green, button.sel.fg => yellow, button.sel.border => yellow, button.sel.inner => yellow +1
   error: bg => green, fg => yellow, line => yellow,
          title.fg => yellow,
          button.bg => green, button.fg => yellow, button.border => yellow,
          button.sel.bg => dkgreen, button.sel.fg => pink, button.sel.border => yellow, button.sel.inner => green +1
  scroll: box => yellow, bg => pink +1, bar => yellow +2
   input: bg => pink, fg => yellow, selbg => dkgreen, selfg => pink, border => green, cursor => dkgreen

    menu: bg => pink, fg => yellow, choice => yellow, icon => yellow,
          selbg => yellow, selfg => pink, selchoice => pink, icon0 => yellow,
          icon0 => yellow, icon1 => dkgreen, icon2 => green, icon3 => pink
  slider: border => yellow, bg => pink, full => yellow
textarea: bg => pink, fg => yellow

# calendar uses "default" for most days, "selected" for selected (duh)
#		"special" is 'today'
box:
	default.bg => pink,
	default.fg => yellow,
	default.border => green,
	selected.bg => yellow,
	selected.fg => pink,
	selected.border => green,
	special.bg => green,
	special.fg => yellow,
	special.border => dkgreen

button:
	default.bg => green,
	default.fg => yellow,
	default.border => yellow,
	selected.bg => yellow,
	selected.fg => pink,
	selected.border => yellow
