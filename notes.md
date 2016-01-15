# WWise vorbis history

* KetnetKick 2
  * Similar to Divinity II, but 0 extra bytes in fmt chunk for channel
    mapping

* Divinity II (PC)
  * 0x28 byte vorb chunk
  * 8 byte packet headers with 32-bit packet size and 32-bit granule
  * setup header has time domain args removed
  * ww2ogg support with no options (detected by vorb size)

* EVE Online Apocrypha (PC)
  * 0x2c byte vorb chunk
  * same as 0x28

* The Saboteur (PC), EVE Online (PC), KOF 12 (360), Mass Effect 2 (360, PC)
  * 0x34 byte vorb chunk (includes stuff that would otherwise go in identification header)
  * 6 byte packet headers (16-bit packet size, 32-bit granule)
  * no identification or comment headers
  * ww2ogg support with --full-setup

* Army of Two: The 40th Day (360, PS3, 13 multiplayer files)
  * stripped out many bits from codebook
  * ww2ogg support with --inline-codebooks

* Assassin's Creed II (360, PS3), Army of Two: The 40th Day (360, all others)
  * moved codebooks to external file, referenced by idx
  * ww2ogg support with no options

* Star Wars: The Force Unleashed II
  * no extra 0x10 bytes in fmt chunk
  * ww2ogg support with no options

* Infamous 2 (PS3)
  * 0x2A byte vorb chunk
  * 2 byte packet headers (16-bit packet size, no granulepos)
  * vorbis packets lack packet type and window flags
  * ww2ogg support with no options (detected by vorb size)

* APB (PC, only certain versions and files?)
  * 0x2A byte vorb chunk merged with 0x18 byte fmt chunk (extraction error?)
  * ww2ogg support with no options (detected by vorb size)

* Rocksmith (360)
  * 0x2A byte vorb chunk merged, but without modified vorb packets
  * detected by a byte in header (though this is a wild guess)

* The Old Republic beta (PC)
  * new packed_codebooks, Wwise 2011.2 uses aoTuV beta 6.03
