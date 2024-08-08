#running in ASCI IMBL/OUTPUT/
for sampledir in  ../input/resolution* ; #for each file in these folders DO in a LOOP, variable name sampledir
 do 
   sample="$(basename $sampledir)" ; #strip leading path from input directory, leaving only directory name as variable name sample
   echo $sample $sampledir ; #display to screen output folder name and input folder name
   mkdir -m 777 $sample ; #make directory with full permissions
   
   #BCT processing pipeline
   imbl-shift.sh -v -b bg_org_pre.tif  -B bg_sft_pre.tif -m mask.tif -a 2512 -f 15 -F 2790 -e 2520 -c -5 -g 23,-73 -C 130,50,0,50  $sampledir/SAMPLE.hdf:/entry/data/data  /dev/shm/clean.hdf:/data ;
   	#-v verbose
	#-b background of unshifted
	#-B background of shifted
	#-m mask for module gaps (expand by 3pixels)
	#-a number of frames for 180deg
	#-f first frame of original pos
	#-F first frame of shifted post
	#-e number of frames to process for shifted pos
	#-c centre of rotation
	#-g gap size (shifted motion in pixels) 
	#-C crop input image
	#input file:/entry/data/data
	#output file:/data

   #copy working clean to sample dir 
   cp /dev/shm/clean.hdf "$sample"/ ;

   #phase contrast retreival
   ctas ipc /dev/shm/clean.hdf:/data -e -v  -z 5500 -d 275.0 -r 75.0 -w 0.3874375 -o /dev/shm/ipced.hdf:/data;
   #input the clean image
   #-e
   #-v verbose
   #-z
   #-d
   #-r
   #-w
   #-o output file in working dir - phase retreived cleaned image

   #ct reconsruction
   ctas ct -k a -a 0.071656 /dev/shm/ipced.hdf:/data:y -o  $sample/rec.hdf:/data -v ;
   #-k a 
   #-a
   #input the phase retreived cleaned image


   rm /dev/shm/* ; #clean working folder
   echo ; #separate input directory processing on screen
 done #end of loop
