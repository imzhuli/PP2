using c-ares to do async dns query
disable c-ares cache, because c-ares only cache fully successful query, but for this service, invalid results should also be cached in case of attack.
successful results timeout is longer than invalid results
