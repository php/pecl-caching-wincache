..\pecl\wincache2\template.rc: ..\pecl\wincache2\wincache_etw.rc

..\pecl\wincache2\wincache_etw.h ..\pecl\wincache2\wincache_etw.rc: ..\pecl\wincache2\wincache_etw.man
	$(MC) -um -e h -h ..\pecl\wincache2\ -r ..\pecl\wincache2\ wincache_etw.man
