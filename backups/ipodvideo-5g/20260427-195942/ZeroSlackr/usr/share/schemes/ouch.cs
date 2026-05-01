\name Ouch!
\def yellow  #FFFF00
\def lime    #00FF00
\def aqua    #99FFFF
\def pink    #FF66FF
\def orange  #FF9933
\def white   #FFFFFF
\def green   #00CC66
\def red     #FF0000
\def dkgreen #00CC00
\def purple  #CC00CC
\def black   #000000 	

  header: bg => orange, fg => pink, line => pink -1, accent => red
	  shadow => white, shine => yellow,
	  gradient.top => lime,
	  gradient.middle => green,
	  gradient.bottom => dkgreen,
	  gradient.bar => yellow
   music: bar => pink
 battery: border => red, bg => lime, fill.normal => yellow +1, fill.low => dkgreen +1, fill.charge => white +1,
	  bg.low => yellow, bg.charging => purple
    lock: border => yellow, fill => white
 loadavg: bg => orange, fg => dkgreen, spike => white

  window: bg => yellow, fg => lime, border => dkgreen -3
  dialog: bg => yellow, fg => black, line => lime,
          title.fg => lime,	
          button.bg => aqua, button.fg => black, button.border => pink,
	  button.sel.bg => pink, button.sel.fg => orange, button.sel.border => green, button.sel.inner => dkgreen +1
   error: bg => gray, fg => black, line => black,
          title.fg => black,
          button.bg => gray, button.fg => black, button.border => black,
          button.sel.bg => dkgreen, button.sel.fg => white, button.sel.border => black, button.sel.inner => green +1
  scroll: box => black, bg => lime +1, bar => dkgreen +2
   input: bg => white, fg => black, selbg => pink, selfg => white, border => lime, cursor => orange

    menu: bg => yellow, fg => lime, choice => dkgreen, icon => black,
          selbg => aqua, selfg => lime, selchoice => aqua, icon0 => black,
          icon0 => black, icon1 => aqua, icon2 => aqua, icon3 => lime
  slider: border => aqua, bg => black, full => orange
textarea: bg => lime, fg => green

# calendar uses "default" for most days, "selected" for selected (duh)
#		"special" is 'today'
box:
	default.bg => lime,
	default.fg => yellow,
	default.border => aqua,
	selected.bg => green,
	selected.fg => lime,
	selected.border => aqua,
	special.bg => aqua,
	special.fg => black,
	special.border => dkgreen

button:
	default.bg => aqua,
	default.fg => black,
	default.border => black,
	selected.bg => green,
	selected.fg => white,
	selected.border => black
