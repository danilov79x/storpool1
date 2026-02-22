cmake -S . -B build && cmake --build build -j  
./build/generate_uint32 -o r1b.bin -n 1000000000 -m random  
./build/unique_count r1b.bin  
progress: processed=35651584/1000000000 (3.57%) unique=35504007 seen_once=35356817  
progress: processed=77594624/1000000000 (7.76%) unique=76898566 seen_once=76206636  
progress: processed=121634816/1000000000 (12.16%) unique=119926844 seen_once=118234765  
progress: processed=164626432/1000000000 (16.46%) unique=161510796 seen_once=158434851  
progress: processed=208666624/1000000000 (20.87%) unique=203678794 seen_once=198771231  
progress: processed=254803968/1000000000 (25.48%) unique=247388302 seen_once=240117418  
progress: processed=296747008/1000000000 (29.67%) unique=286721147 seen_once=276923156  
progress: processed=341835776/1000000000 (34.18%) unique=328580804 seen_once=315672729  
progress: processed=384827392/1000000000 (38.48%) unique=368087928 seen_once=351839990  
progress: processed=429916160/1000000000 (42.99%) unique=409096431 seen_once=388959047  
progress: processed=469762048/1000000000 (46.98%) unique=444983131 seen_once=421090823  
progress: processed=512753664/1000000000 (51.28%) unique=483330460 seen_once=455054788  
progress: processed=553648128/1000000000 (55.36%) unique=519449037 seen_once=486687794  
progress: processed=594542592/1000000000 (59.45%) unique=555230842 seen_once=517691565  
progress: processed=637534208/1000000000 (63.75%) unique=592479868 seen_once=549599506  
progress: processed=679477248/1000000000 (67.95%) unique=628462759 seen_once=580067121  
progress: processed=723517440/1000000000 (72.35%) unique=665869230 seen_once=611365737  
progress: processed=766509056/1000000000 (76.65%) unique=702016733 seen_once=641244916  
progress: processed=808452096/1000000000 (80.85%) unique=736933001 seen_once=669759500  
progress: processed=850395136/1000000000 (85.04%) unique=771509905 seen_once=697657881  
progress: processed=894435328/1000000000 (89.44%) unique=807457517 seen_once=726306314  
progress: processed=938475520/1000000000 (93.85%) unique=843034127 seen_once=754290309  
progress: processed=982515712/1000000000 (98.25%) unique=878252633 seen_once=781637907  
unique=892130949 seen_once=792310384  
