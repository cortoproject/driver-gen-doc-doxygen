#include <driver/fmt/xml/xml.h>
#include <driver/gen/doc/doxygen/doxygen.h>
#include <corto/g/g.h>
#include <parson/parson.h>

typedef struct dg_data {
    corto_xmlnode reader;
    corto_xmlnode node;
    g_file mdFile;
    char *title;
    char *dir;
    int seeAlso;
    bool parsingFluent;
} dg_data;

static
int dg_walkFunctions(
    corto_xmlnode node,
    void *userData);

static
char* dg_descriptionParse(
    char *description)
{
    return corto_strdup(description);
}

static
char* dg_findNodeContent(
    corto_xmlnode node,
    char *name)
{
    const char *elems[CORTO_MAX_SCOPE_DEPTH];
    corto_xmlnode found = node;
    corto_int32 i = 0, count;

    corto_id cpy;
    strcpy(cpy, name);
    count = corto_pathToArray(cpy, elems, "/");

    for (i = 0; found && (i < count); i ++) {
        found = corto_xmlnodeFind(found, elems[i]);
    }

    if (found) {
        return corto_xmlnodeContent(found);
    } else {
        return NULL;
    }
}

static
int dg_walkDetailedDescriptionContent_text(
    corto_xmlnode node,
    void *userData)
{
    dg_data *data = userData;

    char *content = dg_descriptionParse(corto_xmlnodeContent(node));
    g_fileWrite(data->mdFile, "%s\n", content);
    corto_dealloc(content);

    return 1;
}

static
int dg_walkDetailedDescriptionContent_node(
    corto_xmlnode node,
    void *userData)
{
    dg_data *data = userData;
    char *name = (char*)corto_xmlnodeName(node);
    int children = corto_xmlnodeChildrenCount(node);

    if (!strcmp(name, "para") && !children) {
        char *content = dg_descriptionParse(corto_xmlnodeContent(node));
        g_fileWrite(data->mdFile, "\n%s\n\n", content);
        corto_dealloc(content);
    } else if (!strcmp(name, "listitem")) {
        char *content = dg_descriptionParse(corto_xmlnodeContent(node));
        g_fileWrite(data->mdFile, "- %s\n", content);
        corto_dealloc(content);
    } else if (!strcmp(name, "verbatim")) {
        char *content = dg_descriptionParse(corto_xmlnodeContent(node));
        g_fileWrite(data->mdFile, "%s\n", content);
        corto_dealloc(content);
    } else if (!strcmp(name, "ref")) {
        g_fileWrite(data->mdFile, "`%s`", corto_xmlnodeContent(node));
    } else if ((!strcmp(name, "para") ||
                !strcmp(name, "itemizedlist")) &&
                children)
    {
        corto_xmlreaderWalkCallback callbacks[CORTO_XML_CALLBACK_MAX] = {NULL};
        callbacks[CORTO_XML_ELEMENT_NODE] = dg_walkDetailedDescriptionContent_node;
        callbacks[CORTO_XML_TEXT_NODE] = dg_walkDetailedDescriptionContent_text;
        corto_xmlnodeWalkAll(node, callbacks, userData);
        g_fileWrite(data->mdFile, "\n");
    }

    return 1;
}

static
int dg_findParameterList(
    corto_xmlnode node,
    void *userData)
{
    corto_xmlnode *out = userData;
    if (!strcmp(corto_xmlnodeName(node), "para") && corto_xmlnodeChildrenCount(node)) {
        corto_xmlnode paramList = corto_xmlnodeFind(node, "parameterlist");
        if (paramList) {
            *out = paramList;
            return 0;
        }
    }
    return 1;
}

static
int dg_walkParameters(
    corto_xmlnode node,
    void *userData)
{
    dg_data *data = userData;
    if (!strcmp(corto_xmlnodeName(node), "parameteritem")) {
        char *parameterName = dg_findNodeContent(node, "parameternamelist/parametername");
        char *parameterDescription = dg_findNodeContent(node, "parameterdescription/para");
        g_fileWrite(data->mdFile, "#####");
        if (data->parsingFluent) g_fileWrite(data->mdFile, "#");
        g_fileWrite(data->mdFile, "%s\n", parameterName);
        g_fileWrite(data->mdFile, "%s\n\n", parameterDescription);
    }
    return 1;
}

static
int dg_printReturnFindSimple(
    corto_xmlnode node,
    void *userData)
{
    dg_data *data = userData;
    if (!strcmp(corto_xmlnodeName(node), "simplesect")) {
        char *kind = corto_xmlnodeAttrStr(node, "kind");
        if (!strcmp(kind, "return")) {
            char *description = dg_findNodeContent(node, "para");
            g_fileWrite(data->mdFile, "#####");
            if (data->parsingFluent) g_fileWrite(data->mdFile, "#");
            g_fileWrite(data->mdFile, "return\n");
            g_fileWrite(data->mdFile, "%s\n\n", description);
            goto found;
        }
    }
    return 1;
found:
    return 0;
}

static
int dg_printReturn(
    corto_xmlnode node,
    void *userData)
{
    dg_data *data = userData;
    if (!strcmp(corto_xmlnodeName(node), "para")) {
        if (!corto_xmlnodeWalkChildren(node, dg_printReturnFindSimple, data)) {
            goto found;
        }
    }
    return 1;
found:
    return 0;
}

static
int dg_printSeeAlsoFindSimpleRef(
    corto_xmlnode node,
    void *userData)
{
    dg_data *data = userData;
    if (!strcmp(corto_xmlnodeName(node), "ref")) {
        char *func = corto_xmlnodeContent(node);
        g_fileWrite(data->mdFile, "[%s](#%s_%s)\n", func, data->title, func);
    }
    return 1;
}

static
int dg_printSeeAlsoFindSimple(
    corto_xmlnode node,
    void *userData)
{
    dg_data *data = userData;
    if (!strcmp(corto_xmlnodeName(node), "simplesect")) {
        char *kind = corto_xmlnodeAttrStr(node, "kind");
        if (!strcmp(kind, "see")) {
            corto_xmlnode para = corto_xmlnodeFind(node, "para");
            if (para) {
                if (!data->seeAlso) {
                    g_fileWrite(data->mdFile, "####");
                    if (data->parsingFluent) g_fileWrite(data->mdFile, "#");
                    g_fileWrite(data->mdFile, "See also\n");
                }
                corto_xmlnodeWalkChildren(para, dg_printSeeAlsoFindSimpleRef, data);
                data->seeAlso ++;
            }
        }
    }
    return 1;
}

static
int dg_printSeeAlso(
    corto_xmlnode node,
    void *userData)
{
    dg_data *data = userData;
    if (!strcmp(corto_xmlnodeName(node), "para")) {
        if (!corto_xmlnodeWalkChildren(node, dg_printSeeAlsoFindSimple, data)) {
            goto found;
        }
    }
    return 1;
found:
    return 0;
}

static
int16_t dg_printFluent(
    char *name,
    char *returnType,
    dg_data *data)
{
    corto_id fluentDefFile;
    char ch, *ptr, *bptr;

    strcpy(fluentDefFile, data->dir);
    strcat(fluentDefFile, "/struct");
    bptr = &fluentDefFile[strlen(fluentDefFile)];

    for (ptr = returnType; (ch = *ptr); ptr++) {
        if (ch == '_') {
            bptr[0] = '_';
            bptr[1] = '_';
            bptr ++;
        } else {
            bptr[0] = ch;
        }
        bptr ++;
    }
    bptr[0] = '\0';

    strcat(fluentDefFile, ".xml");

    corto_xmlreader reader = corto_xmlreaderNew(fluentDefFile, "doxygen");
    if (reader) {
        corto_xmlnode root = corto_xmlreaderRoot(reader);
        corto_xmlnode compounddef = corto_xmlnodeFind(root, "compounddef");
        corto_xmlnode sectiondef = corto_xmlnodeFind(compounddef, "sectiondef");
        dg_data fluentData = *data;
        fluentData.parsingFluent = true;
        fluentData.node = sectiondef;

        g_fileWrite(data->mdFile, "### %s fluent methods\n", name);
        g_fileWrite(data->mdFile, "The following methods extend the functionality of `%s` by\n", name);
        g_fileWrite(data->mdFile, "appending them to a call like this: `%s(...).<method>(...)`.\n", name);
        g_fileWrite(data->mdFile, "Multiple methods can be appended to the call, as long as the\n");
        g_fileWrite(data->mdFile, "previous method returns an instance of `%s`.\n", returnType);

        if (!corto_xmlnodeWalkChildren(fluentData.node, dg_walkFunctions, &fluentData)) {
            corto_throw("error(s) occurred while parsing");
            goto error;
        }

        corto_xmlreaderFree(reader);
    } else {
        corto_throw("invalid doxygen file");
        goto error;
    }

    return 0;
error:
    return -1;
}

static
int dg_walkFunction(
    corto_xmlnode node,
    void *userData)
{
    dg_data *data = userData;

    if (!strcmp(corto_xmlnodeName(node), "memberdef")) {
        char *name = dg_findNodeContent(node, "name");
        if (name[0] == '_') name ++;

        char *returnType = dg_findNodeContent(node, "definition");

        /* Strip name */
        if (!data->parsingFluent) {
            *strrchr(returnType, ' ') = '\0';
        } else {
            *strchr(returnType, '(') = '\0';
        }

        /* Strip prefixes (like export macro's) */
        char *definitionStart = strrchr(returnType, ' ');
        if (definitionStart) {
            char *argsStart = strchr(returnType, '(');
            if (argsStart && (definitionStart > argsStart)) {
                definitionStart = argsStart;
            }
            returnType = definitionStart + 1;
        }

        char *briefdescription = dg_findNodeContent(node, "briefdescription/para");
        corto_xmlnode detailedDescription = corto_xmlnodeFind(node, "detaileddescription");

        if (briefdescription) {
            char *argsstring = dg_findNodeContent(node, "argsstring");


            /* Doxygen has some trouble with inline function pointers */
            if (argsstring[0] == ')') argsstring ++;
            char *tmp = strreplace(argsstring, "(", "(\n    ");
            argsstring = strreplace(tmp, ", ", ",\n    ");
            corto_dealloc(tmp);

            g_fileWrite(data->mdFile, "###");
            if (data->parsingFluent) g_fileWrite(data->mdFile, "#");
            g_fileWrite(data->mdFile, "%s\n", name);

            g_fileWrite(data->mdFile, "%s\n\n", briefdescription);
            if (detailedDescription) {
                corto_xmlreaderWalkCallback callbacks[CORTO_XML_CALLBACK_MAX] = {NULL};
                callbacks[CORTO_XML_ELEMENT_NODE] = dg_walkDetailedDescriptionContent_node;
                callbacks[CORTO_XML_TEXT_NODE] = dg_walkDetailedDescriptionContent_text;
                corto_xmlnodeWalkAll(detailedDescription, callbacks, data);
            }

            g_fileWrite(data->mdFile, "####");
            if (data->parsingFluent) g_fileWrite(data->mdFile, "#");
            g_fileWrite(data->mdFile, "Signature\n\n```c\n%s\n%s%s\n```\n\n", returnType, name, argsstring);

            corto_xmlnode paramList = NULL;
            corto_xmlnodeWalkChildren(detailedDescription, dg_findParameterList, &paramList);
            if (paramList) {
                corto_xmlnodeWalkChildren(paramList, dg_walkParameters, data);
            }

            /* Add return */
            corto_xmlnodeWalkChildren(detailedDescription, dg_printReturn, data);

            /* Obtain returntype, add fluent description if applicable */
            if (!data->parsingFluent) {
                int returnTypeLength = strlen(returnType),
                    fluentLength = strlen("__fluent");
                if (returnTypeLength > fluentLength) {
                    if (!strcmp(&returnType[returnTypeLength - fluentLength], "__fluent")) {
                        if (dg_printFluent(name, returnType, data)) {
                            goto error;
                        }
                    }
                }
            }

            /* Add see also */
            data->seeAlso = 0;
            corto_xmlnodeWalkChildren(detailedDescription, dg_printSeeAlso, data);
            if (data->seeAlso) {
                g_fileWrite(data->mdFile, "\n");
            }
        }
    }

    return 1;
error:
    return 0;
}

static
int dg_walkFunctions(
    corto_xmlnode node,
    void *userData)
{
    dg_data *data = userData;

    if (!data->parsingFluent && !strcmp(corto_xmlnodeName(node), "sectiondef")) {
        char *kind = corto_xmlnodeAttrStr(node, "kind");
        if (!strcmp(kind, "func")) {
            if (!corto_xmlnodeWalkChildren(node, dg_walkFunction, data)) {
                goto error;
            }
        }
    } else if (data->parsingFluent && !strcmp(corto_xmlnodeName(node), "memberdef")) {
        char *kind = corto_xmlnodeAttrStr(node, "kind");
        if (!strcmp(kind, "variable")) {
            if (!dg_walkFunction(node, data)) {
                goto error;
            }
        }
    }

    return 1;
error:
    return 0;
}

void dg_fileNameToHeader(
    char *fileName)
{
    char ch, *ptr;
    for (ptr = fileName; (ch = *ptr); ptr++) {
        if (ch == '_') {
            *ptr = ' ';
        }
    }
    char *slash = strrchr(fileName, '/');
    if (slash) {
        memmove(fileName, slash + 1, strlen(slash));
    }
}

int16_t dg_toMd(
    g_generator g,
    g_file outFile,
    char *dir,
    const char *xml_chapter)
{
    dg_data data;
    corto_id chapter;

    char *xmlFile = corto_asprintf("%s/%s_8h.xml", dir, xml_chapter);
    strcpy(chapter, xml_chapter);
    dg_fileNameToHeader(chapter);

    data.mdFile = outFile;

    /* Load reader */
    data.reader = corto_xmlreaderNew(xmlFile, "doxygen");
    if (data.reader) {
        data.title = chapter;
        data.dir = dir;
        data.parsingFluent = false;

        corto_xmlnode root = corto_xmlreaderRoot(data.reader);
        corto_xmlnode compounddef = corto_xmlnodeFind(root, "compounddef");
        char *brief = dg_findNodeContent(compounddef, "briefdescription/para");
        if (brief) {
            corto_xmlnode detail = corto_xmlnodeFind(compounddef, "detaileddescription");
            corto_xmlnode sect1 = corto_xmlnodeFind(detail, "sect1");
            if (sect1) {
                char *title = dg_findNodeContent(sect1, "title");
                if (title) {
                    g_fileWrite(data.mdFile, "## %s\n\n", title);
                } else {
                    g_fileWrite(data.mdFile, "## %s\n\n", chapter);
                }
                detail = sect1;
            } else {
                g_fileWrite(data.mdFile, "## %s\n\n", chapter);
            }
            g_fileWrite(data.mdFile, "[%s]\n\n", brief);
            if (detail) {
                corto_xmlreaderWalkCallback callbacks[CORTO_XML_CALLBACK_MAX] = {NULL};
                callbacks[CORTO_XML_ELEMENT_NODE] = dg_walkDetailedDescriptionContent_node;
                callbacks[CORTO_XML_TEXT_NODE] = dg_walkDetailedDescriptionContent_text;
                if (!corto_xmlnodeWalkAll(detail, callbacks, &data)) {
                    goto error;
                }
            }
        } else {
            g_fileWrite(data.mdFile, "## %s\n\n", chapter);
        }

        data.node = compounddef;
        if (!corto_xmlnodeWalkChildren(data.node, dg_walkFunctions, &data)) {
            corto_throw("error(s) occurred while parsing");
            goto error;
        }

        corto_xmlreaderFree(data.reader);
    } else {
        corto_throw("invalid doxygen file");
        goto error;
    }

    return 0;
error:
    return -1;
}

static
int16_t dg_walkDocuments(
    g_generator g,
    JSON_Array *documents)
{
    int i;
    for (i = 0; i < json_array_get_count(documents); i++) {
        JSON_Object *json_o_doc = json_array_get_object(documents, i);
        const char *title = json_object_get_string(json_o_doc, "title");
        JSON_Array *chapters = json_object_get_array(json_o_doc, "chapters");

        /* Get markdown filename by replacing whitespaces with underscores */
        char *md_filename = corto_asprintf("doc/%s.md", corto_strdup(title));
        char *ptr, ch;
        for (ptr = md_filename; (ch = *ptr); ptr++) {
            if (isspace(ch)) {
                *ptr = '_';
            }
        }

        g_file md = g_fileOpen(g, md_filename);
        if (!md) {
            corto_throw("failed to open markdownn file '%s'");
            goto error;
        }
        g_fileWrite(md, "# %s\n", title);

        if (chapters) {
            int c;
            for (c = 0; c < json_array_get_count(chapters); c ++) {
                const char *chapter = json_array_get_string(chapters, c);
                char *xml = corto_asprintf(".doxygen/%s_8h.xml", chapter);
                if (dg_toMd(g, md, ".doxygen", chapter)) {
                    goto error;
                }
                free(xml);
            }
        } else {
            corto_warning(
                "empty header section for document '%s' project.json", title);
        }
        free(md_filename);
    }

    return 0;
error:
    return -1;
}

int genmain(
    g_generator g)
{
    int8_t ret;

    /* Run doxygen command */
    if (corto_proc_cmd(
        strarg("doxygen %s/Doxyfile", DRIVER_GEN_DOC_DOXYGEN_ETC), &ret) || ret)
    {
        corto_throw("failed to run doxygen in '%s'", corto_cwd());
        goto error;
    }

    /* Look for doxygen sections in the project.json */
    JSON_Value *json = NULL;

    json = json_parse_file("project.json");
    if (json) {
        JSON_Object *json_o = json_value_get_object(json);
        if (!json_o) {
            corto_throw("invalid JSON in project.json: expected object");
            goto error;
        }

        JSON_Array *json_a_doxy = json_object_dotget_array(json_o, "value.doxygen");
        if (json_a_doxy) {
            /* Walk documents in doxygen configuration, generate markdown */
            if (dg_walkDocuments(g, json_a_doxy)) {
                corto_throw("markdown from generated doxygen XML failed");
                json_value_free(json);
                goto error;
            }
        }

        json_value_free(json);
    } else {
        /* No project.json, skip */
    }

    return 0;
error:
    return -1;
}
