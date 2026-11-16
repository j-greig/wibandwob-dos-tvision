#!/bin/sh
# A short text animation, recorded as casts/bounce.cast (docs/terminal-transport.md): a ball
# bounces inside a box for six seconds, leaving a fading trail, with a frame counter.
W=36
H=8
x=3; y=2; dx=1; dy=1
px=0; py=0; qx=0; qy=0
f=0
printf '\033[2J\033[H\033[?25l'
edge="+"
i=0
while [ $i -lt $W ]; do edge="$edge-"; i=$((i+1)); done
edge="$edge+"
printf '\033[1;1H%s' "$edge"
i=2
while [ $i -le $((H + 1)) ]; do
  printf '\033[%d;1H|%*s|' $i $W ''
  i=$((i + 1))
done
printf '\033[%d;1H%s' $((H + 2)) "$edge"
while [ $f -lt 48 ]; do
  [ $qx -gt 0 ] && printf '\033[%d;%dH ' $((qy + 1)) $((qx + 1))
  [ $px -gt 0 ] && printf '\033[%d;%dH.' $((py + 1)) $((px + 1))
  printf '\033[%d;%dHo' $((y + 1)) $((x + 1))
  printf '\033[%d;1Hframe %2d of 48   x=%2d y=%d ' $((H + 3)) $f $x $y
  qx=$px; qy=$py; px=$x; py=$y
  x=$((x + dx)); y=$((y + dy))
  [ $x -ge $W ] && dx=-1
  [ $x -le 1 ] && dx=1
  [ $y -ge $H ] && dy=-1
  [ $y -le 1 ] && dy=1
  f=$((f + 1))
  sleep 0.125
done
printf '\033[%d;1Hthe end%*s\n' $((H + 3)) 24 ''
