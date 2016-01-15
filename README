ww2ogg
======

Audiokinetic Wwise RIFF/RIFX Vorbis to Ogg Vorbis converter by hcs

Usage
----
Standard usage is just

`ww2ogg input.bin`

which will convert input.bin to input.ogg.

You can also specify an output file with `-o`, as in

`ww2ogg input.ogg -o output.ogg`


Troubleshooting
--------------------------------------------------------------------------------

* If the conversion seemed to go well but you get a nonsense output file
  * first try setting the alternate packed codebooks:
    
    `ww2ogg input.ogg --pcb packed_codebooks_aoTuV_603.bin`
  
  * then try also setting `--no-mod-packets`:
  
    `ww2ogg input.ogg --no-mod-packets --pcb packed_codebooks_aoTuV_603.bin`

  * You can try other combinations of `--pcb`, `--no-mod-packets`,
    and `--mod-packets`, but these are the common ones that work.

* `Parse error: expected 0x42 fmt if vorb missing` suggests that the input is
   not Vorbis data at all, and so it is not supported by this program.

* `Error opening packed_codebooks.bin` means the ww2ogg couldn't find the
   packed_codebooks.bin file that comes with the program. Either run ww2ogg
   in the same working directory as packed_codebooks.bin, or give the path with
   the `--pcb` switch

   `other_dir/ww2ogg input.ogg --pcb other_dir/packed_codebooks.bin`

* `Parse error: invalid codebook id 0x342, try --full-setup`

   follow the suggestion and use `--full-setup`


Note:

It is a good idea to run the output through revorb to get smaller,
cleaner files than ww2ogg generates currently.
http://www.hydrogenaudio.org/forums/lofiversion/index.php/t64328.html
https://ipfs.io/ipfs/QmVgjfU7qgPEtANatrfh7VQJby9t1ojrTbN7X8Ei4djF4e/revorb.exe
