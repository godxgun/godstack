{{- if partial "is-public-doc.html" . -}}
# {{ .Title }}

Documentation: {{ .Permalink }}
{{ with .Description }}Description: {{ . }}{{ end }}

{{ partial "markdown-content.html" . }}
{{- end -}}
