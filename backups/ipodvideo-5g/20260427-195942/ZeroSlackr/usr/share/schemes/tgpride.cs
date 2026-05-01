\name Transgender Pride
\def black  #000000
\def white  #ffffff
\def blue   #66ffff
\def pink   #ff33dd
\def green  #00ff00
\def dkgreen#00cc00

  header: bg => @tgpride-(header).png, fg => black, line => blue -1, accent => green
	  shadow => dkgreen, shine => green,
	  gradient.top => pink,
	  gradient.middle => pink,
	  gradient.bottom => dkgreen,
	  gradient.bar => green
   music: bar => dkgreen
 battery: border => pink, bg => blue, fill.normal => pink +1, fill.low => dkgreen +1, fill.charge => pink +1,
	  bg.low => blue, bg.charging => green, chargingbolt => blue
    lock: border => blue, fill => blue
 loadavg: bg => green, fg => dkgreen, spike => pink

  window: bg => pink, fg => blue, border => green -3
  dialog: bg => pink, fg => blue, line => blue,
          title.fg => blue,	
          button.bg => pink, button.fg => blue, button.border => green,
	  button.sel.bg => green, button.sel.fg => blue, button.sel.border => blue, button.sel.inner => blue +1
   error: bg => green, fg => blue, line => blue,
          title.fg => blue,
          button.bg => green, button.fg => blue, button.border => blue,
          button.sel.bg => dkgreen, button.sel.fg => pink, button.sel.border => blue, button.sel.inner => green +1
  scroll: box => blue, bg => pink +1, bar => blue +2
   input: bg => pink, fg => blue, selbg => dkgreen, selfg => pink, border => green, cursor => dkgreen

    menu: bg => pink, fg => blue, choice => blue, icon => blue,
          selbg => blue, selfg => pink, selchoice => pink, icon0 => blue,
          icon0 => blue, icon1 => dkgreen, icon2 => green, icon3 => pink
  slider: border => blue, bg => pink, full => blue
textarea: bg => pink, fg => blue

# calendar uses "default" for most days, "selected" for selected (duh)
#		"special" is 'today'
box:
	default.bg => pink,
	default.fg => blue,
	default.border => green,
	selected.bg => blue,
	selected.fg => pink,
	selected.border => green,
	special.bg => green,
	special.fg => blue,
	special.border => dkgreen

button:
	default.bg => green,
	default.fg => blue,
	default.border => blue,
	selected.bg => blue,
	selected.fg => pink,
	selected.border => blue
