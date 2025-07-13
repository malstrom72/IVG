@ECHO OFF
CD /D "%~dp0"

where pika >NUL 2>&1
IF ERRORLEVEL 1 (
    ECHO PikaScript not found. Install it first.
    GOTO error
)
where pandoc >NUL 2>&1
IF ERRORLEVEL 1 (
    ECHO pandoc not found. Install it first.
    GOTO error
)

pika updateDocumentationImages.pika "..\docs\IVG Documentation.md" || GOTO error
pandoc -s -o "..\docs\ImpD Documentation.html" --metadata title="ImpD Documentation" --include-in-header pandoc.css "..\docs\ImpD Documentation.md" || GOTO error
pandoc -s -o "..\docs\IVG Documentation.html" --metadata title="IVG Documentation" --include-in-header pandoc.css "..\docs\IVG Documentation.md" || GOTO error

EXIT /b 0
:error
EXIT /b %ERRORLEVEL%
