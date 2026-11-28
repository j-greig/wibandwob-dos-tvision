# The jelly layout: thirteen beings on a 150x43 terminal, back window first.
# Replay on the open page: while read -r l; do case "$l" in cmd:*) curl -s -X POST --data-binary "$l" http://127.0.0.1:8137/ctl;; esac; done < unkernel/layouts/jelly.ctl
cmd:open_primer path=being-flyer-left.txt x=6 y=3 w=16 h=6 colour=cyan title=flyer%20left
cmd:open_primer path=being-wobbler.txt x=23 y=6 w=16 h=6 colour=magenta title=wobbler
cmd:open_primer path=being-robot-tv.txt x=38 y=0 w=19 h=14 colour=green title=robot%20tv
cmd:open_primer path=being-jelly.txt x=60 y=2 w=34 h=19 colour=blue title=jelly
cmd:open_primer path=being-niff-small.txt x=97 y=1 w=16 h=6 colour=black title=niff%20small
cmd:open_primer path=being-flyer-right.txt x=134 y=3 w=16 h=6 colour=cyan title=flyer%20right
cmd:open_primer path=figure-wib-standing-large.txt x=60 y=24 w=16 h=17 colour=magenta title=wib%20standing%20large
cmd:open_primer path=figure-wob-standing-large.txt x=76 y=24 w=16 h=17 colour=cyan title=wob%20standing%20large
cmd:open_primer path=scramble-sitting.txt x=92 y=35 w=16 h=6 colour=grey title=sitting
cmd:open_primer path=being-mech.txt x=109 y=30 w=21 h=11 colour=red title=mech
cmd:open_primer path=place-castle-tower-www-flag.txt x=133 y=24 w=16 h=17 colour=grey title=castle%20tower%20www%20flag
cmd:open_primer path=being-gary-the-donkey-antlers.txt x=1 y=15 w=57 h=26 colour=brown title=gary%20the%20donkey%20antlers
cmd:open_primer path=being-void-creature.txt x=100 y=6 w=28 h=17 colour=black title=void%20creature
