# Memtree

_Memtree is a modern memory monitor tool._

Its primary features include:
 - **Recursive memory summation.** This allows you to see memory usage including child processes for any given process. This is very useful for:
 
   - Telling what application is eating all your RAM, even if it has spawned many childen and the memory usage is split between them.
   
   - Telling how much memory a whole application is using, even if it has spawned many children - for instance if you want to know how much memory closing your browser will free up, it is now as simple as finding the main browser process - the usage of all its children is included in the value, giving you a convient total.
   
 - **Searchable tree.** You can quickly and easily find any process with a case-insensitive search, just by typing.
 
 - **In-depth measurements.** Memtree gives values for all of the following:
 
   - USS (Unique Set Size) - this is the sum of private mappings
   
   - PSS (Proportional Set Size) - this is the USS, plus the size of each shared mapping divided by how many times it is shared. It represents how much of memory usage is due to a given process, taking into account the fact that shared memory is reused for multiple programs. **This is the value to use for judging memory usage.**
   
   - RSS (Resident Set Size) - this is the USS, plus the size of all shared mappings. It is the total size of mappings for a process, but is an overestimate for memory usage since dynamic libraries can be shared between processes.
   
   - Swap - this is how much of swap usage is due to the process (like RSS). Swap is when your hard drive or SSD is used for memory, normally when you begin to run out of RAM.
   
   - Swap PSS - this is like PSS, but for swap usage. **This is the value to use for judging swap usage.**
   
   - Total USS - this is USS + Swap PSS (since Swap USS is not available from the Linux kernel).
   
   - Total PSS - this is PSS + Swap PSS.
   
 - **Fast and modern interface using GTK4.**
