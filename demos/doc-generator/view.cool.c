void Api(char *name, char *func, char *desc) {
    cool_html_raw(COOL_SV("    <div class=\"api-card\">\n        <h3 class=\"api-name\">"));
    cool_html_txt(name, strlen(name));
    cool_html_raw(COOL_SV("</h3>\n        <code class=\"language-c\">"));
    cool_html_txt(func, strlen(func));
    cool_html_raw(COOL_SV(" </code>\n        <div class=\"api-description\">\n            "));
    cool_html_txt(desc, strlen(desc));
    cool_html_raw(COOL_SV("\n        </div>\n    </div>\n"));
}

