..\pecl\wincache\template.rc: ..\pecl\wincache\wincache_etw.rc

..\pecl\wincache\wincache_etw.h ..\pecl\wincache\wincache_etw.rc: ..\pecl\wincache\wincache_etw.man
	$(MC) -um -e h -h ..\pecl\wincache\ -r ..\pecl\wincache\ wincache_etw.man
